#pragma once

#include "cell.h"
#include "common.h"

#include <functional>

class Sheet : public SheetInterface {
public:
    ~Sheet();

    void SetCell(Position pos, std::string text) override;

    const CellInterface* GetCell(Position pos) const override;
    CellInterface* GetCell(Position pos) override;

    void ClearCell(Position pos) override;

    Size GetPrintableSize() const override;

    void PrintValues(std::ostream& output) const override;
    void PrintTexts(std::ostream& output) const override;

    // Можете дополнить ваш класс нужными полями и методами


private:
    // Можете дополнить ваш класс нужными полями и методами
    std::vector<std::vector<std::unique_ptr<Cell>>> data_{0};

    enum PrintType {VALUES, TEXT};
    void PrintRow(std::ostream& os,
                  int row,
                  int len,
                  PrintType type) const;
    void IsValidPosition(Position pos) const;
    void Resize(Position pos);

};
