using System.Globalization;
using ClosedXML.Excel;

internal sealed record CsvTable(string Name, IReadOnlyList<string[]> Rows);

internal static class WorkbookParser
{
    private sealed record ParsedTable(CsvTable Csv, string WorkbookName, string Source,
        FieldDeclaration[] Fields, Dictionary<long, int> PrimaryKeyRows);

    private static readonly string[] MetadataLabels =
    [
        "#Table", "#Target", "", "#Key", "#Scope", "#Type", "#Required",
        "#Min", "#Max", "#Default", "#Allowed", "#Reference", "#Description", "#Field",
    ];

    // 통합 문서 이름이 아니라 각 데이터 시트의 이름을 CSV 이름으로 사용한다.
    public static IReadOnlyList<CsvTable> ReadDirectory(string excelDirectory)
    {
        var tables = new List<ParsedTable>();
        var tablesByName = new Dictionary<string, ParsedTable>(StringComparer.OrdinalIgnoreCase);
        var paths = Directory.EnumerateFiles(excelDirectory, "*.xlsx", SearchOption.TopDirectoryOnly)
            .Where(path => !Path.GetFileName(path).StartsWith("~$", StringComparison.Ordinal))
            .OrderBy(path => path, StringComparer.Ordinal);

        foreach (string path in paths)
        {
            string workbookName = Path.GetFileNameWithoutExtension(path);
            using XLWorkbook workbook = OpenWorkbook(path);
            foreach (IXLWorksheet worksheet in workbook.Worksheets)
            {
                string source = $"{Path.GetFileName(path)}:{worksheet.Name}";
                ParsedTable? table = ReadWorksheet(worksheet, workbookName, source);
                if (table == null)
                {
                    continue;
                }

                if (!tablesByName.TryAdd(table.Csv.Name, table))
                {
                    string prior = tablesByName[table.Csv.Name].Source;
                    throw new InvalidDataException($"{source}!B1: Duplicate table name '{table.Csv.Name}', already used by {prior}.");
                }

                tables.Add(table);
            }
        }

        if (tables.Count == 0)
        {
            throw new InvalidDataException($"{excelDirectory}: No game data sheets were found.");
        }

        ValidateReferences(tables, tablesByName);
        return tables.Select(table => table.Csv).ToArray();
    }

    private static XLWorkbook OpenWorkbook(string path)
    {
        try
        {
            return new XLWorkbook(path);
        }
        catch (Exception exception)
        {
            throw new InvalidDataException($"{Path.GetFileName(path)}: Could not read workbook: {exception.Message}", exception);
        }
    }

    private static ParsedTable? ReadWorksheet(IXLWorksheet worksheet, string workbookName, string source)
    {
        // 실제 값으로 행·열 범위를 잡는다. 서식과 빈 문자열은 범위를 늘리지 않는다.
        var values = new Dictionary<(int Row, int Column), string>();
        int lastRow = MetadataLabels.Length;
        int lastColumn = 2;
        foreach (IXLCell cell in worksheet.CellsUsed(XLCellsUsedOptions.AllContents))
        {
            if (cell.HasFormula)
            {
                throw new InvalidDataException($"{source}!{cell.Address}: Formula cells are not allowed.");
            }
            if (cell.DataType == XLDataType.Error)
            {
                throw new InvalidDataException($"{source}!{cell.Address}: Excel error cells are not allowed.");
            }

            string value = GetCellText(cell);
            if (value.Length == 0)
            {
                continue;
            }

            int row = cell.Address.RowNumber;
            int column = cell.Address.ColumnNumber;
            values.Add((row, column), value);
            lastRow = Math.Max(lastRow, row);
            lastColumn = Math.Max(lastColumn, column);
        }

        if (values.Count == 0)
        {
            return null;
        }

        string CellText(int row, int column) => values.GetValueOrDefault((row, column), string.Empty);

        for (int i = 0; i < MetadataLabels.Length; ++i)
        {
            if (!string.Equals(CellText(i + 1, 1), MetadataLabels[i], StringComparison.Ordinal))
            {
                string expected = MetadataLabels[i].Length == 0 ? "a blank cell" : $"'{MetadataLabels[i]}'";
                throw new InvalidDataException($"{source}!A{i + 1}: Expected {expected}.");
            }
        }

        string tableName = CellText(1, 2);
        if (!IsTableName(tableName) || !string.Equals(tableName, worksheet.Name, StringComparison.Ordinal))
        {
            throw new InvalidDataException($"{source}!B1: #Table must be a valid CSV file name and match the worksheet name.");
        }
        if (CellText(2, 2) is not ("Shared" or "Server" or "Client"))
        {
            throw new InvalidDataException($"{source}!B2: #Target must be Shared, Server or Client.");
        }

        var fields = new HashSet<string>(StringComparer.Ordinal);
        var declarations = new FieldDeclaration[lastColumn - 1];
        bool hasPrimaryKey = false;
        for (int column = 2; column <= lastColumn; ++column)
        {
            int headerRow = MetadataLabels.Length;
            string name = CellText(headerRow, column);
            if (!IsIdentifier(name) || !fields.Add(name))
            {
                throw new InvalidDataException($"{source}!{worksheet.Cell(headerRow, column).Address}: Invalid or duplicate #Field.");
            }
            FieldDeclaration declaration = FieldDeclarations.Read(worksheet, source, values, column, hasPrimaryKey);
            declarations[column - 2] = declaration;
            hasPrimaryKey |= declaration.IsPrimaryKey;
        }
        if (!hasPrimaryKey)
        {
            throw new InvalidDataException($"{source}!A4: #Key must declare one Primary column.");
        }

        // 메타데이터와 내부의 빈 행·열은 건너뛰지 않는다.
        var rows = new List<string[]>(lastRow);
        var primaryKeyRows = new Dictionary<long, int>();
        for (int row = 1; row <= lastRow; ++row)
        {
            var cells = new string[lastColumn];
            for (int column = 1; column <= lastColumn; ++column)
            {
                cells[column - 1] = CellText(row, column);
            }
            if (row > MetadataLabels.Length && cells.Any(text => text.Length != 0))
            {
                ValidateDataRow(worksheet, source, row, cells, declarations, primaryKeyRows);
            }
            rows.Add(cells);
        }

        return new ParsedTable(new CsvTable(tableName, rows), workbookName, source, declarations, primaryKeyRows);
    }

    // PK 중복은 테이블 단위로 검사하고, 선택 컬럼의 원본 빈칸은 CSV에 보존한다.
    private static void ValidateDataRow(IXLWorksheet worksheet, string source, int row, string[] cells,
        FieldDeclaration[] declarations, Dictionary<long, int> primaryKeyRows)
    {
        for (int column = 2; column <= cells.Length; ++column)
        {
            FieldDeclaration declaration = declarations[column - 2];
            IXLCell cell = worksheet.Cell(row, column);
            if (declaration.ReadDataValue(cell, source, cells[column - 1]) is not long number)
            {
                continue;
            }

            if (declaration.IsPrimaryKey && !primaryKeyRows.TryAdd(number, row))
            {
                var priorAddress = worksheet.Cell(primaryKeyRows[number], column).Address;
                throw new InvalidDataException($"{source}!{cell.Address}: Duplicate primary key {number}, first used at {priorAddress}.");
            }
            cells[column - 1] = number.ToString(CultureInfo.InvariantCulture);
        }
    }

    // 모든 시트의 PK를 모은 뒤 참조 선언, 기본값, 실제 키를 확인한다.
    private static void ValidateReferences(IReadOnlyList<ParsedTable> tables, IReadOnlyDictionary<string, ParsedTable> tablesByName)
    {
        foreach (ParsedTable table in tables)
        {
            for (int i = 0; i < table.Fields.Length; ++i)
            {
                FieldDeclaration field = table.Fields[i];
                string reference = field.Reference;
                if (reference.Length == 0)
                {
                    continue;
                }

                string column = XLHelper.GetColumnLetterFromNumber(i + 2);
                InvalidDataException Error(int row, string reason) => new($"{table.Source}!{column}{row}: {reason} '{reference}'.");
                string[] parts = reference.Split('.');
                if (!tablesByName.TryGetValue(parts[1], out ParsedTable? target) ||
                    target.WorkbookName != parts[0] || target.Csv.Name != parts[1])
                {
                    throw Error(FieldDeclarations.ReferenceRow, "#Reference target workbook or sheet was not found:");
                }

                int targetColumn = Array.IndexOf(target.Csv.Rows[MetadataLabels.Length - 1], parts[2]);
                if (targetColumn < 1)
                {
                    throw Error(FieldDeclarations.ReferenceRow, "#Reference target field was not found:");
                }
                FieldDeclaration targetField = target.Fields[targetColumn - 1];
                if (!targetField.IsPrimaryKey)
                {
                    throw Error(FieldDeclarations.ReferenceRow, "#Reference target field must be the primary key:");
                }
                if (field.Type != targetField.Type)
                {
                    throw Error(FieldDeclarations.ReferenceRow, $"#Reference type must match {targetField.Type}:");
                }

                if (field.DefaultValue is long defaultKey && !target.PrimaryKeyRows.ContainsKey(defaultKey))
                {
                    throw Error(FieldDeclarations.DefaultRow, $"#Default #Reference key {defaultKey} was not found in");
                }

                for (int row = MetadataLabels.Length; row < table.Csv.Rows.Count; ++row)
                {
                    string value = table.Csv.Rows[row][i + 1];
                    if (value.Length != 0 && !target.PrimaryKeyRows.ContainsKey(long.Parse(value, CultureInfo.InvariantCulture)))
                    {
                        throw Error(row + 1, $"#Reference key {value} was not found in");
                    }
                }
            }
        }
    }

    private static string GetCellText(IXLCell cell)
    {
        return cell.DataType switch
        {
            XLDataType.Blank => string.Empty,
            XLDataType.Boolean => cell.GetBoolean() ? "true" : "false",
            XLDataType.Number => cell.GetDouble().ToString("R", CultureInfo.InvariantCulture),
            XLDataType.DateTime => cell.GetDateTime().ToString("O", CultureInfo.InvariantCulture),
            XLDataType.TimeSpan => cell.GetTimeSpan().ToString("c", CultureInfo.InvariantCulture),
            XLDataType.Text => cell.GetString().ReplaceLineEndings("\n"),
            _ => cell.GetString(),
        };
    }

    private static bool IsTableName(string name)
    {
        if (!IsIdentifier(name))
        {
            return false;
        }

        string upper = name.ToUpperInvariant();
        return upper is not ("CON" or "PRN" or "AUX" or "NUL") &&
            !(upper.Length == 4 && upper[3] is >= '0' and <= '9' &&
            (upper.StartsWith("COM", StringComparison.Ordinal) || upper.StartsWith("LPT", StringComparison.Ordinal)));
    }

    internal static bool IsIdentifier(string name) => name.Length != 0 && (char.IsLetter(name[0]) || name[0] == '_') &&
        name.Any(char.IsLetterOrDigit) && name.All(c => char.IsLetterOrDigit(c) || c == '_');
}
