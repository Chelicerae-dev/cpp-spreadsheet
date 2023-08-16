#include "formula.h"

#include "FormulaAST.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <sstream>
#include <regex>

using namespace std::literals;

std::ostream& operator<<(std::ostream& output, FormulaError fe) {
    return output << fe.ToString();
}

namespace {
class Formula : public FormulaInterface {
public:
    explicit Formula(std::string expression);
    Value Evaluate(const SheetInterface& sheet) const override;
    std::string GetExpression() const override;
    std::vector<Position> GetReferencedCells() const override;

private:
    FormulaAST ast_;
    std::vector<Position> referenced_cells_;
};
}  // namespace

std::unique_ptr<FormulaInterface> ParseFormula(std::string expression) {
    try {
        auto result = std::make_unique<Formula>(std::move(expression));
        return std::move(result);
    } catch (const std::exception& ex) {
        throw FormulaException(ex.what());
    }
}

Formula::Formula(std::string expression)
    : ast_(ParseFormulaAST(std::move(expression))),
      referenced_cells_(ast_.GetCells().begin(), ast_.GetCells().end())
{
    if(!referenced_cells_.empty()) {
        //Убираем дубли и сортируем чтобы при вызове GetReferencedCells отдавать готовое
        auto unq_last = std::unique(referenced_cells_.begin(), referenced_cells_.end());
        referenced_cells_.erase(unq_last, referenced_cells_.end());
        std::sort(referenced_cells_.begin(), referenced_cells_.end());
    }
}

Formula::Value Formula::Evaluate(const SheetInterface& sheet) const {
    try {
        auto getter = [&sheet](const Position& position) {
            const CellInterface* cell = sheet.GetCell(position);
            if(cell == nullptr) return 0.;
            auto result = cell->GetValue();
            if(std::holds_alternative<double>(result)) {
                return std::get<double>(result);
            } else if(std::holds_alternative<std::string>(result)) {
                //Спасибо за то, что 3D преобразуется в 3., ведь все мы любим регулярки (которые внезапно не такие и простые для double)
                std::regex double_reg("^(-?)(0|([1-9][0-9]*))(\\.[0-9]+)?$");
                std::smatch match_result;
                try {
                if(std::regex_match(std::get<std::string>(result).cbegin(), std::get<std::string>(result).cend(), match_result, double_reg)) {
                    return std::stod(std::get<std::string>(result));
                } else {
                    throw std::invalid_argument("");
                }
//                try {
//                    return std::stod(std::get<std::string>(result));
                } catch(...) {
                    throw FormulaError(FormulaError::Category::Value);
                }
            } else {
                throw std::get<FormulaError>(result);
            }
        };
        return ast_.Execute(getter);
    } catch(const FormulaError& err) {
        return err;
    }
}

std::string Formula::GetExpression() const {
    std::ostringstream result;
    ast_.PrintFormula(result);
    return result.str();
}

std::vector<Position> Formula::GetReferencedCells() const {
    return referenced_cells_;
}
