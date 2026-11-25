using System.Globalization;
using ClosedXML.Excel;

internal sealed record DataColumn(string Name, string MemberName, string GetterName, string Scope, FieldDeclaration Field);
internal sealed record DataRow(int RecordNumber, string?[] Values);
internal sealed record DataTable(string Name, string Target, DataColumn[] Columns, List<DataRow> Rows,
    int KeyIndex, Dictionary<uint, int> PrimaryKeys)
{
    public string Location(int column, int row) => $"{Name}.csv!{XLHelper.GetColumnLetterFromNumber(column + 2)}{row}:";

    public bool IncludesTarget(string target) => Target == "Shared" || Target == target;
}

internal static class CsvDataTables
{
    public static DataTable Read(string directory, string name, EnumDefinitions enumDefinitions)
    {
        string source = name + ".csv";
        List<string[]> rows = CsvReader.Read(Path.Combine(directory, source));
        string[] labels = WorkbookParser.MetadataLabels;
        if (rows.Count < labels.Length || rows[0].Length < 2 || rows[0].Length > 16384)
        {
            throw new InvalidDataException($"{source}!A1: Missing table metadata or invalid column count.");
        }
        int width = rows[0].Length;
        for (int row = 0; row < rows.Count; ++row)
        {
            if (rows[row].Length != width)
            {
                throw new InvalidDataException($"{source}!A{row + 1}: Expected {width} CSV fields.");
            }
            string expected = row < labels.Length ? labels[row] : string.Empty;
            if (rows[row][0] != expected)
            {
                throw new InvalidDataException($"{source}!A{row + 1}: Expected '{expected}' in the metadata column.");
            }
        }
        if (rows[0][1] != name)
        {
            throw new InvalidDataException($"{source}!B1: #Table must match the selected CSV name.");
        }
        string target = rows[1][1];
        if (target is not ("Shared" or "Server" or "Client"))
        {
            throw new InvalidDataException($"{source}!B2: #Target must be Shared, Server or Client.");
        }
        for (int row = 0; row < 3; ++row)
        {
            for (int column = row == 2 ? 1 : 2; column < width; ++column)
            {
                if (rows[row][column].Length != 0)
                {
                    throw new InvalidDataException($"{source}!{XLHelper.GetColumnLetterFromNumber(column + 1)}{row + 1}: Expected a blank cell.");
                }
            }
        }

        var metadata = new Dictionary<(int, int), string>();
        for (int row = 0; row < labels.Length; ++row)
        {
            for (int column = 0; column < width; ++column)
            {
                metadata.Add((row + 1, column + 1), rows[row][column]);
            }
        }
        var columns = new DataColumn[width - 1];
        var memberNames = new HashSet<string>(StringComparer.Ordinal);
        int keyIndex = -1;
        for (int index = 0; index < columns.Length; ++index)
        {
            string fieldName = rows[labels.Length - 1][index + 1];
            string memberName = fieldName.Length == 0 ? string.Empty : "m_" + char.ToLowerInvariant(fieldName[0]) + fieldName[1..];
            if (!EnumDefinitions.IsCppName(fieldName) || !memberNames.Add(memberName))
            {
                throw new InvalidDataException($"{source}!{XLHelper.GetColumnLetterFromNumber(index + 2)}{labels.Length}: Invalid or duplicate C++ #Field name.");
            }
            string getterName = "Get" + char.ToUpperInvariant(fieldName[0]) + fieldName[1..];
            // Windows 헤더가 치환하는 대표적인 Get 이름은 생성하지 않는다.
            if (getterName is "GetMessage" or "GetObject" or "GetRValue" or "GetGValue" or "GetBValue")
            {
                throw new InvalidDataException($"{source}!{XLHelper.GetColumnLetterFromNumber(index + 2)}{labels.Length}: #Field getter '{getterName}' conflicts with a Windows macro.");
            }
            FieldDeclaration field = FieldDeclarations.Read(null, source, metadata, index + 2, keyIndex != -1, enumDefinitions);
            columns[index] = new DataColumn(fieldName, memberName, getterName, rows[4][index + 1], field);
            if (field.IsPrimaryKey)
            {
                keyIndex = index;
            }
        }
        if (keyIndex == -1)
        {
            throw new InvalidDataException($"{source}!A4: #Key must declare one Primary column.");
        }

        var table = new DataTable(name, target, columns, [], keyIndex, []);
        for (int row = labels.Length; row < rows.Count; ++row)
        {
            if (rows[row].All(text => text.Length == 0))
            {
                continue;
            }
            var values = new string?[columns.Length];
            for (int index = 0; index < columns.Length; ++index)
            {
                values[index] = columns[index].Field.ReadCsvValue(rows[row][index + 1], table.Location(index, row + 1))?.Text;
            }
            uint key = uint.Parse(values[keyIndex]!, NumberStyles.None, CultureInfo.InvariantCulture);
            if (!table.PrimaryKeys.TryAdd(key, row + 1))
            {
                throw new InvalidDataException($"{table.Location(keyIndex, row + 1)} Duplicate primary key {key}, first used at {table.Location(keyIndex, table.PrimaryKeys[key])}");
            }
            table.Rows.Add(new DataRow(row + 1, values));
        }
        return table;
    }

    // CSV에는 통합 문서명이 없으므로 0.3에서 검증한 한정자 대신 고유한 시트명으로 대상 CSV를 찾는다.
    public static void ValidateReferences(IReadOnlyDictionary<string, DataTable> tables)
    {
        foreach (DataTable table in tables.Values)
        {
            for (int index = 0; index < table.Columns.Length; ++index)
            {
                DataColumn column = table.Columns[index];
                string reference = column.Field.Reference;
                if (reference.Length == 0)
                {
                    continue;
                }
                InvalidDataException Error(int row, string reason) => new($"{table.Location(index, row)} #Reference {reason}");
                string[] parts = reference.Split('.');
                if (!tables.TryGetValue(parts[1], out DataTable? target))
                {
                    throw Error(FieldDeclarations.ReferenceRow, $"target table '{parts[1]}' is not selected.");
                }
                DataColumn targetKey = target.Columns[target.KeyIndex];
                if (targetKey.Name != parts[2] || targetKey.Field.Type != column.Field.Type)
                {
                    throw Error(FieldDeclarations.ReferenceRow, $"must name a primary key of the same type: '{reference}'.");
                }
                if (column.Field.DefaultValue is FieldValue defaultValue && !ContainsKey(target, defaultValue.Text))
                {
                    throw Error(FieldDeclarations.DefaultRow, $"default key {defaultValue.Text} does not exist in '{target.Name}'.");
                }
                foreach (DataRow row in table.Rows)
                {
                    if (row.Values[index] is string value && !ContainsKey(target, value))
                    {
                        throw Error(row.RecordNumber, $"key {value} does not exist in '{target.Name}'.");
                    }
                }
                if (IsProjected(table, column, "Server") && !target.IncludesTarget("Server"))
                {
                    throw Error(FieldDeclarations.ReferenceRow, $"target table '{target.Name}' is absent from Server output.");
                }
                if (IsProjected(table, column, "Client") && !target.IncludesTarget("Client"))
                {
                    throw Error(FieldDeclarations.ReferenceRow, $"target table '{target.Name}' is absent from Client output.");
                }
            }
        }
    }

    public static bool IsProjected(DataTable table, DataColumn column, string target) =>
        table.IncludesTarget(target) && (column.Scope == "Shared" || column.Scope == target);

    private static bool ContainsKey(DataTable table, string value) =>
        table.PrimaryKeys.ContainsKey(uint.Parse(value, NumberStyles.None, CultureInfo.InvariantCulture));
}
