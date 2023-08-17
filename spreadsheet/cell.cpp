#include "cell.h"

#include <cassert>
#include <iostream>
#include <string>
#include <optional>

class Cell::Impl {
public:
    using Value = CellInterface::Value;
    enum CellType { EMPTY, TEXT, FORMULA };

    explicit Impl(CellType type) : type_(type) {}
    virtual ~Impl() = default;

    virtual Value GetValue() const = 0;
    virtual std::string GetText() const = 0;

    CellType GetType() const {
        return type_;
    }

private:
    CellType type_;

};

class Cell::EmptyImpl : public Cell::Impl {
public:
    explicit EmptyImpl() : Impl(Impl::CellType::EMPTY) {}

    ~EmptyImpl() override = default;

    Value GetValue() const override {
        return 0.;
    }
    std::string GetText() const override {
        return "";
    }

};

class Cell::TextImpl : public Cell::Impl  {
public:
    explicit TextImpl(const std::string& str)
        : Impl(Impl::CellType::TEXT),
          value_(str) {}

    ~TextImpl() override = default;

    Value GetValue() const override {
        if(value_[0] == '\'') return value_.substr(1);
        return value_;
    }

    std::string GetText() const override {
        return value_;
    }


private:
    std::string value_;
};

class Cell::FormulaImpl : public Cell::Impl {
public:
    explicit FormulaImpl(const std::string& str, SheetInterface& sheet)
        : Impl(Impl::CellType::FORMULA),
          formula_(ParseFormula(str)),
          sheet_(sheet){}

    ~FormulaImpl() override = default;

    Value GetValue() const override {
        if(cache_.has_value()) {
            return cache_.value();
        }

        auto result = formula_->Evaluate(sheet_);
        if(std::holds_alternative<double>(result)) {
            cache_ = std::get<double>(result);
            return cache_.value();
        } else {
            return std::get<FormulaError>(result);
        }
    }

    std::string GetText() const override {
        std::string result = "=";
        return result.append(formula_->GetExpression());
    }

    std::vector<Position> GetReferences() const {
        return formula_->GetReferencedCells();
    }

    void InvalidateCache() {
        cache_.reset();
    }

private:
    std::unique_ptr<FormulaInterface> formula_ = nullptr;
    //Используем ссылку из-за интерфейса в задании
    SheetInterface& sheet_;
    mutable std::optional<double> cache_;


};

Cell::Cell(SheetInterface& sheet)
    : sheet_(sheet){}

Cell::~Cell() {}

void Cell::Set(std::string text) {
    std::unique_ptr<Impl> temp = CreateTempCell(text, sheet_);
    if(temp->GetType() == Impl::CellType::FORMULA) {
        InitializeNewCells(dynamic_cast<FormulaImpl*>(temp.get())->GetReferences());
        std::unordered_set<Cell*> visited{this};
        if(HasCyclic(this, GetFormula(temp.get()), visited)) {
            throw CircularDependencyException("Circular dependency detected");
        } else {
            impl_ = std::move(temp);
        }
        ResetDescending(GetFormula(impl_.get()));
    } else {
        impl_ = std::move(temp);
        ClearDescending();
    }
    std::unordered_set<Cell*> visited{this};
    InvalidateAscendingCaches(visited);
}

void Cell::Clear() {
    //Использование ClearDescending позволяет избежать лишнего создания временного объекта
    ClearDescending();
    std::unordered_set<Cell*> visited{this};
    InvalidateAscendingCaches(visited);
    impl_.release();
}

std::vector<Position> Cell::GetReferencedCells() const {
    if(impl_ == nullptr || impl_->GetType() != Impl::CellType::FORMULA) {
        return std::vector<Position>();
    }
    return GetFormula(impl_.get())->GetReferences();
}

Cell::Value Cell::GetValue() const {
    return impl_->GetValue();
}
std::string Cell::GetText() const {
    return impl_->GetText();
}

void Cell::InvalidateAscendingCaches(std::unordered_set<Cell*>& visited) {
    if(impl_->GetType() == Impl::CellType::FORMULA) {
        GetFormula(impl_.get())->InvalidateCache();
    }
    for(Cell* cell : ascending_) {
        if(visited.count(cell) == 0) {
            visited.insert(cell);
            cell->InvalidateAscendingCaches(visited);
        } else {
            continue;
        }
    }
}

std::unique_ptr<Cell::Impl> Cell::CreateTempCell(const std::string& text, SheetInterface& sheet) {
    Cell result(sheet);
    if(text.size() > 0) {
        if(text[0] == FORMULA_SIGN && text.size() > 1) {
            try {
                return std::make_unique<FormulaImpl>(text.substr(1), sheet_);
            } catch(const std::exception& ex) {
                throw FormulaException(ex.what());
            }
        } else {
            return std::make_unique<TextImpl>(text);
        }
    } else {
        return std::make_unique<EmptyImpl>();
    }
}

void Cell::InitializeNewCells(const std::vector<Position>& positions) {
    for(const Position& pos : positions) {
        if(sheet_.GetCell(pos) == nullptr) {
            sheet_.SetCell(pos, "");
        }
    }
}

bool Cell::HasCyclic(const Cell* target, Impl* current, std::unordered_set<Cell*>& visited) const {
    const FormulaImpl* formula = GetFormula(current);

    //Нет формулы - нет циклических зависимостей
    if(formula == nullptr) {
        return false;
    }

    for(const Position& position : formula->GetReferences()) {
        Cell* temp = dynamic_cast<Cell*>(sheet_.GetCell(position));
        if(temp == target) return true;
        if(visited.count(temp) == 0) {            
            visited.insert(temp);
            if(temp->HasCyclic(target, GetFormulaImpl(temp), visited)) return true;
        }
    }

    return false;
}

Cell::FormulaImpl* Cell::GetFormulaImpl(Cell* cell) const {
    if(cell->impl_->GetType() == Impl::CellType::FORMULA) {
        auto* result = dynamic_cast<FormulaImpl*>(cell->impl_.get());
        return result;
    } else {
        return nullptr;
    }
}

Cell::FormulaImpl* Cell::GetFormula(Impl* impl) const {
    if(impl == nullptr) return nullptr;
    if(impl->GetType() == Impl::CellType::FORMULA) {
        return dynamic_cast<FormulaImpl*>(impl);
    } else {
        return nullptr;
    }
}
void Cell::ClearDescending() {
    if(!descending_.empty()) {
        for(Cell* cell : descending_) {
            cell->ascending_.erase(this);
        }
        descending_.clear();
    }
}

void Cell::ResetDescending(const FormulaImpl* formula) {
    ClearDescending();
    for(Position pos : formula->GetReferences()) {
        //немного писанины для удобства понимания происходящего
        CellInterface* cell_interface = sheet_.GetCell(pos);
        Cell* cell = cell_interface == nullptr ? nullptr : dynamic_cast<Cell*>(cell_interface);
        descending_.insert(cell);
        //добавляем нашу ячейку в ascending у "последователя"
        cell->ascending_.insert(this);
    }
}
