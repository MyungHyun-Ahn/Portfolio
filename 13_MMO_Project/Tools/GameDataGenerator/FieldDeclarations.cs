using System.Globalization;
using ClosedXML.Excel;

internal static class FieldDeclarations
{
    // 데이터 값을 검증하기 전에 컬럼 선언만 확인한다.
    public static bool Check(IXLWorksheet worksheet, string source, IReadOnlyDictionary<(int, int), string> values,
        int column, bool hasPrimaryKey)
    {
        string Text(int row) => values.GetValueOrDefault((row, column), string.Empty);
        InvalidDataException Error(int row, string reason) =>
            new($"{source}!{worksheet.Cell(row, column).Address}: {values[(row, 1)]} {reason}");

        string key = Text(4);
        if (key is not ("" or "Primary") || (key == "Primary" && hasPrimaryKey))
        {
            throw Error(4, "must be blank or the only Primary declaration.");
        }

        string scope = Text(5);
        if (scope is not ("Shared" or "Server" or "Client" or "Ignore"))
        {
            throw Error(5, "must be Shared, Server, Client or Ignore.");
        }

        string type = Text(6);
        if (type is not ("int32" or "uint32"))
        {
            throw Error(6, "must be int32 or uint32.");
        }

        string required = Text(7);
        if (required is not ("true" or "false"))
        {
            throw Error(7, "must be true or false.");
        }

        if (key == "Primary" && (scope != "Shared" || type != "uint32" || required != "true"))
        {
            int row = scope != "Shared" ? 5 : type != "uint32" ? 6 : 7;
            throw Error(row, "must satisfy #Key=Primary: Shared, uint32 and required.");
        }

        long? ReadOptionalInteger(int row)
        {
            string text = Text(row);
            if (text.Length == 0)
            {
                return null;
            }
            if (!TryReadInteger(type, text, out long number))
            {
                throw Error(row, $"must be a valid {type}.");
            }
            return number;
        }

        long? minimum = ReadOptionalInteger(8);
        long? maximum = ReadOptionalInteger(9);
        if (key == "Primary" && minimum == 0)
        {
            throw Error(8, "must be at least 1 for #Key=Primary.");
        }
        if (key == "Primary" && maximum == 0)
        {
            throw Error(9, "must be at least 1 for #Key=Primary.");
        }
        if (minimum > maximum)
        {
            throw Error(9, "cannot be less than #Min.");
        }
        if (Text(10).Length != 0 && required == "true")
        {
            throw Error(10, "is only valid for an optional field.");
        }
        ReadOptionalInteger(10);

        if (Text(11).Length != 0)
        {
            var allowed = new HashSet<long>();
            foreach (string item in Text(11).Split('|', StringSplitOptions.TrimEntries))
            {
                if (!TryReadInteger(type, item, out long number) || !allowed.Add(number))
                {
                    throw Error(11, $"contains an invalid or duplicate {type} value.");
                }
            }
        }

        if (Text(12).Length != 0 && !IsReferenceSyntax(Text(12)))
        {
            throw Error(12, "must use Workbook.Sheet.Field with no file extension.");
        }
        return hasPrimaryKey || key == "Primary";
    }

    private static bool TryReadInteger(string type, string value, out long number)
    {
        if (type == "uint32")
        {
            bool parsed = uint.TryParse(value, NumberStyles.None, CultureInfo.InvariantCulture, out uint unsigned);
            number = unsigned;
            return parsed;
        }

        bool signedParsed = int.TryParse(value, NumberStyles.AllowLeadingSign, CultureInfo.InvariantCulture, out int signed);
        number = signed;
        return signedParsed;
    }

    private static bool IsReferenceSyntax(string value)
    {
        string[] parts = value.Split('.');
        return parts.Length == 3 && !string.IsNullOrWhiteSpace(parts[0]) && parts[0] == parts[0].Trim() &&
            parts[0].IndexOfAny(['/', '\\', ':', '*', '?', '"', '<', '>', '|']) < 0 && !parts[0].Any(char.IsControl) &&
            WorkbookParser.IsIdentifier(parts[1]) && WorkbookParser.IsIdentifier(parts[2]);
    }
}
