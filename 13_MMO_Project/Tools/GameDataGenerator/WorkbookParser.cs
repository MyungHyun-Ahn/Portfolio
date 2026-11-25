using System.Globalization;
using ClosedXML.Excel;

internal sealed record CsvTable(string Name, IReadOnlyList<string[]> Rows);

internal static class WorkbookParser
{
    private static readonly string[] MetadataLabels =
    [
        "#Table", "#Target", "", "#Key", "#Scope", "#Type", "#Required",
        "#Min", "#Max", "#Default", "#Allowed", "#Reference", "#Description", "#Field",
    ];

    // 통합 문서 이름이 아니라 각 데이터 시트의 이름을 CSV 이름으로 사용한다.
    public static IReadOnlyList<CsvTable> ReadDirectory(string excelDirectory)
    {
        var tables = new List<CsvTable>();
        var sources = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        var paths = Directory.EnumerateFiles(excelDirectory, "*.xlsx", SearchOption.TopDirectoryOnly)
            .Where(path => !Path.GetFileName(path).StartsWith("~$", StringComparison.Ordinal))
            .OrderBy(path => path, StringComparer.Ordinal);

        foreach (string path in paths)
        {
            using XLWorkbook workbook = OpenWorkbook(path);
            foreach (IXLWorksheet worksheet in workbook.Worksheets)
            {
                string source = $"{Path.GetFileName(path)}:{worksheet.Name}";
                CsvTable? table = ReadWorksheet(worksheet, source);
                if (table == null)
                {
                    continue;
                }

                if (sources.TryGetValue(table.Name, out string? prior))
                {
                    throw new InvalidDataException($"{source}!B1: Duplicate table name '{table.Name}', already used by {prior}.");
                }

                sources.Add(table.Name, source);
                tables.Add(table);
            }
        }

        if (tables.Count == 0)
        {
            throw new InvalidDataException($"{excelDirectory}: No game data sheets were found.");
        }

        return tables;
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

    private static CsvTable? ReadWorksheet(IXLWorksheet worksheet, string source)
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
        bool hasPrimaryKey = false;
        for (int column = 2; column <= lastColumn; ++column)
        {
            int headerRow = MetadataLabels.Length;
            string name = CellText(headerRow, column);
            if (!IsIdentifier(name) || !fields.Add(name))
            {
                throw new InvalidDataException($"{source}!{worksheet.Cell(headerRow, column).Address}: Invalid or duplicate #Field.");
            }
            hasPrimaryKey = FieldDeclarations.Check(worksheet, source, values, column, hasPrimaryKey);
        }
        if (!hasPrimaryKey)
        {
            throw new InvalidDataException($"{source}!A4: #Key must declare one Primary column.");
        }

        // 메타데이터와 내부의 빈 행·열은 건너뛰지 않는다.
        var rows = new List<string[]>(lastRow);
        for (int row = 1; row <= lastRow; ++row)
        {
            var cells = new string[lastColumn];
            for (int column = 1; column <= lastColumn; ++column)
            {
                cells[column - 1] = CellText(row, column);
            }
            rows.Add(cells);
        }

        return new CsvTable(tableName, rows);
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
