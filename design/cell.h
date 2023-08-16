#pragma once

#include "common.h"
#include "formula.h"

#include <functional>
#include <unordered_set>
#include <optional>

class Sheet;

class Cell : public CellInterface {
private:
    class Impl;
    class EmptyImpl;
    class TextImpl;
    class FormulaImpl;
public:
    Cell(SheetInterface& sheet);
    ~Cell();

    void Set(std::string text);
    void Clear();

    Value GetValue() const override;
    std::string GetText() const override;
    std::vector<Position> GetReferencedCells() const override;

    bool IsReferenced() const;

    void InvalidateAscendingCaches(std::unordered_set<Cell*>& visited);
    //хелпер для получения доступа к FormulaImpl в случае, если impl_ является его экземпляром
    FormulaImpl* GetFormulaImpl(Cell* cell) const;
    FormulaImpl* GetFormula(Impl* impl) const;


private:
    std::unique_ptr<Impl> impl_;
    SheetInterface& sheet_;

    //храним цепочку зависимостей "вверх" и "вниз" для работы с графом
    //Храним в Cell, а не FormulaImpl так как FormulaImpl может измениться на другой тип и потерять зависимости
    //делаем mutable для доступа из других ячеек (для заполнения ascending)
    //через неё мы сможем инвалидировать кеш, удалять нужые зависимости при изменении и пересчитывать кеш
    mutable std::unordered_set<Cell*> ascending_;
    mutable std::unordered_set<Cell*> descending_;

    //Set() helpers
    std::unique_ptr<Impl> CreateTempCell(const std::string& text, SheetInterface& sheet);
    void InitializeNewCells(const std::vector<Position>& positions);
    //target - это this, в аргументах нужна для передачи её рекурсивно далее
    bool HasCyclic(const Cell* target, Impl* current, std::unordered_set<Cell*>& visited) const;
    //очищаем список ячеек, от которых зависим если значение более не формула
    void ClearDescending();
    //переопределяем список ячеек, от которых зависим в случае новой формулы
    void ResetDescending(const FormulaImpl* formula);
};
