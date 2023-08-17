#include "sheet.h"

#include "cell.h"
#include "common.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <optional>
#include <sstream>

using namespace std::literals;


Sheet::~Sheet() {}

void Sheet::SetCell(Position pos, std::string text) {
    IsValidPosition(pos);
    CellInterface* cell = GetCell(pos);
    if(cell == nullptr) {
        Resize(pos);
        data_[pos.row][pos.col] = std::make_unique<Cell>(*this);
        SetCell(pos, text);
    } else {
        dynamic_cast<Cell*>(cell)->Set(text);
    }
}

const CellInterface* Sheet::GetCell(Position pos) const {
    IsValidPosition(pos);
    if(static_cast<int>(data_.size()) <= pos.row || static_cast<int>(data_[pos.row].size()) <= pos.col) return nullptr;
    return data_[pos.row][pos.col].get();
}
CellInterface* Sheet::GetCell(Position pos) {
    IsValidPosition(pos);
    if(static_cast<int>(data_.size()) <= pos.row || static_cast<int>(data_[pos.row].size()) <= pos.col) return nullptr;
    return data_[pos.row][pos.col].get();
}

void Sheet::ClearCell(Position pos) {
    IsValidPosition(pos);
    Cell* cell = dynamic_cast<Cell*>(GetCell(pos));
    if(cell == nullptr ) return;
    cell->Clear();
    data_[pos.row][pos.col] = nullptr;
}

Size Sheet::GetPrintableSize() const {
    int x = 0, y = 0;
    for(int i = 0; i < static_cast<int>(data_.size()); ++i) {
        for(int j = 0; j < static_cast<int>(data_[i].size()); ++j) {
            if(GetCell({i, j}) != nullptr) {
                x = std::max(i + 1, x);
                y = std::max(j + 1, y);
            }
        }
    }
    return {x, y};
}

void Sheet::PrintValues(std::ostream& output) const {
    Size size = GetPrintableSize();
    for(int i = 0; i < size.rows; ++i) {
            PrintRow(output, i, size.cols, PrintType::VALUES);
            output << '\n';
    }
}
void Sheet::PrintTexts(std::ostream& output) const {
    Size size = GetPrintableSize();
    for(int i = 0; i < size.rows; ++i) {
            PrintRow(output, i, size.cols, PrintType::TEXT);
            output << '\n';
    }
}

std::unique_ptr<SheetInterface> CreateSheet() {
    return std::make_unique<Sheet>();
}

void Sheet::PrintRow(std::ostream& os,
                     int row,
                     int len,
                     PrintType type) const
{
    using namespace std::string_literals;
    bool first = true;
    for(int i = 0; i < len; ++i) {
        std::stringstream val;
        const CellInterface* cell = GetCell({row, i});
        if(cell != nullptr) {
            if(type == PrintType::TEXT) {
                val << cell->GetText();
            } else {
                if(std::holds_alternative<std::string>(cell->GetValue())) {
                    std::string temp = std::get<std::string>(cell->GetValue());
                    if(temp[0] == '\'') temp = temp.substr(1);
                    val << temp;
                } else if(std::holds_alternative<double>(cell->GetValue())) {
                    val << std::get<double>(cell->GetValue());
                } else {
                    val << std::get<FormulaError>(cell->GetValue());
                }
            }

        } else {
            val << "";
        }
        if(first) {
            first = false;
            os << val.str();
        } else {
            os << '\t' << val.str();
        }
    }
}

void Sheet::IsValidPosition(Position pos) const {
    if(pos.row >= Position::MAX_ROWS || pos.row < 0 || pos.col >= Position::MAX_COLS || pos.col < 0) throw InvalidPositionException("Invalid position passed");
}

void Sheet::Resize(Position pos) {
    IsValidPosition(pos);
    if(static_cast<int>(data_.size()) <= pos.row) data_.resize(pos.row + 1);
    if(static_cast<int>(data_[pos.row].size()) <= pos.col) data_[pos.row].resize(pos.col + 1);
}
