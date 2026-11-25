using System.Globalization;
using ClosedXML.Excel;

// double은 검증된 32비트 정수와 float32를 손실 없이 비교한다. 문자열은 Text만 사용한다.
internal readonly record struct FieldValue(string Text, double Number = 0);

internal sealed record FieldDeclaration(string Type, bool IsPrimaryKey, bool IsRequired,
    FieldValue? Minimum, FieldValue? Maximum, HashSet<string>? Allowed, FieldValue? DefaultValue, string Reference)
{
    // CSV로 변환하기 전에 원본 셀의 종류를 확인한다.
    public FieldValue? ReadDataValue(IXLCell cell, string source, string text)
    {
        InvalidDataException Error(string reason) => new($"{source}!{cell.Address}: {reason}");
        if (text.Length == 0)
        {
            if (IsRequired)
            {
                throw Error("#Required must have a value in the Excel cell.");
            }
            return null;
        }

        bool isString = Type == "string";
        if (cell.DataType != (isString ? XLDataType.Text : XLDataType.Number))
        {
            throw Error($"{Type} requires a {(isString ? "text" : "numeric")} Excel cell.");
        }

        FieldValue? value = isString ? FieldDeclarations.ReadValue(Type, text) : FieldDeclarations.ReadNumber(Type, cell.GetDouble());
        if (value is not FieldValue parsed)
        {
            throw Error($"Value must be a valid {Type}.");
        }

        if (IsPrimaryKey && parsed.Number == 0)
        {
            throw Error("#Key=Primary must be at least 1.");
        }
        if (GetConstraintError(parsed) is string reason)
        {
            throw Error($"Value {reason}");
        }
        return parsed;
    }

    public string? GetConstraintError(FieldValue value)
    {
        if (Minimum is FieldValue minimum && value.Number < minimum.Number)
        {
            return $"#Min violation: must be at least {minimum.Text}.";
        }
        if (Maximum is FieldValue maximum && value.Number > maximum.Number)
        {
            return $"#Max violation: must be at most {maximum.Text}.";
        }
        if (Allowed != null && !Allowed.Contains(value.Text))
        {
            return "#Allowed violation: value is not in the allowed list.";
        }
        return null;
    }
}

internal static class FieldDeclarations
{
    internal const int DefaultRow = 10;
    internal const int ReferenceRow = 12;

    // 컬럼 선언을 검사하고 float32 숫자 셀 메타데이터는 CSV에 같은 값으로 남긴다.
    public static FieldDeclaration Read(IXLWorksheet worksheet, string source, Dictionary<(int, int), string> values,
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
        if (type is not ("int32" or "uint32" or "float32" or "string"))
        {
            throw Error(6, "must be int32, uint32, float32 or string.");
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

        FieldValue? ReadMetadataValue(int row, string text)
        {
            IXLCell cell = worksheet.Cell(row, column);
            if (type == "string" && cell.DataType != XLDataType.Text)
            {
                throw Error(row, "requires a text Excel cell.");
            }
            if (type == "float32" && cell.DataType == XLDataType.Number)
            {
                FieldValue? value = ReadNumber(type, cell.GetDouble());
                if (value is FieldValue parsed)
                {
                    values[(row, column)] = parsed.Text;
                }
                return value;
            }
            return ReadValue(type, text);
        }

        FieldValue? ReadOptionalValue(int row)
        {
            string text = Text(row);
            return text.Length == 0 ? null : ReadMetadataValue(row, text) ?? throw Error(row, $"must be a valid {type}.");
        }

        if (type == "string" && (Text(8).Length != 0 || Text(9).Length != 0))
        {
            throw Error(Text(8).Length != 0 ? 8 : 9, "is only valid for a numeric field.");
        }
        FieldValue? minimum = ReadOptionalValue(8);
        FieldValue? maximum = ReadOptionalValue(9);
        if (key == "Primary" && minimum?.Number == 0)
        {
            throw Error(8, "must be at least 1 for #Key=Primary.");
        }
        if (key == "Primary" && maximum?.Number == 0)
        {
            throw Error(9, "must be at least 1 for #Key=Primary.");
        }
        if (minimum?.Number > maximum?.Number)
        {
            throw Error(9, "cannot be less than #Min.");
        }
        if (Text(DefaultRow).Length != 0 && required == "true")
        {
            throw Error(DefaultRow, "is only valid for an optional field.");
        }
        FieldValue? defaultValue = ReadOptionalValue(DefaultRow);

        HashSet<string>? allowed = null;
        if (Text(11).Length != 0)
        {
            allowed = new HashSet<string>(StringComparer.Ordinal);
            StringSplitOptions options = type == "string" ? StringSplitOptions.None : StringSplitOptions.TrimEntries;
            foreach (string item in Text(11).Split('|', options))
            {
                if (ReadMetadataValue(11, item) is not FieldValue value || !allowed.Add(value.Text))
                {
                    throw Error(11, $"contains an invalid or duplicate {type} value.");
                }
            }
        }

        string reference = Text(ReferenceRow);
        if (reference.Length != 0 && !IsReferenceSyntax(reference))
        {
            throw Error(ReferenceRow, "must use Workbook.Sheet.Field with no file extension.");
        }

        var declaration = new FieldDeclaration(type, key == "Primary", required == "true", minimum, maximum, allowed,
            defaultValue, reference);
        if (defaultValue is FieldValue parsed && declaration.GetConstraintError(parsed) is string reason)
        {
            throw Error(DefaultRow, reason);
        }
        return declaration;
    }

    internal static FieldValue? ReadValue(string type, string text)
    {
        if (text.Length == 0 || text.Contains('\0'))
        {
            return null;
        }
        if (type == "string")
        {
            return new FieldValue(text);
        }
        if (type == "uint32")
        {
            return uint.TryParse(text, NumberStyles.None, CultureInfo.InvariantCulture, out uint number) ?
                new FieldValue(number.ToString(CultureInfo.InvariantCulture), number) : null;
        }
        if (type == "int32")
        {
            return int.TryParse(text, NumberStyles.AllowLeadingSign, CultureInfo.InvariantCulture, out int number) ?
                new FieldValue(number.ToString(CultureInfo.InvariantCulture), number) : null;
        }
        if (type != "float32")
        {
            return null;
        }

        const NumberStyles FloatStyles = NumberStyles.AllowLeadingSign | NumberStyles.AllowDecimalPoint | NumberStyles.AllowExponent;
        // 십진 텍스트를 double을 거쳐 읽으면 단정밀도 중점 부근에서 다르게 반올림될 수 있다.
        if (!float.TryParse(text, FloatStyles, CultureInfo.InvariantCulture, out float value) || !float.IsFinite(value))
        {
            return null;
        }
        // 0이 아닌 가수가 단정밀도에서 0으로 반올림되면 거절한다.
        if (value == 0)
        {
            foreach (char c in text)
            {
                if (c is 'e' or 'E')
                {
                    break;
                }
                if (c is >= '1' and <= '9')
                {
                    return null;
                }
            }
        }
        return FormatFloat32(value);
    }

    internal static FieldValue? ReadNumber(string type, double value)
    {
        if (type == "float32")
        {
            float number = (float)value;
            if (!float.IsFinite(number) || (value != 0 && number == 0))
            {
                return null;
            }
            return FormatFloat32(number);
        }
        if (type is not ("int32" or "uint32"))
        {
            return null;
        }

        bool isUnsigned = type == "uint32";
        long minimum = isUnsigned ? 0L : int.MinValue;
        long maximum = isUnsigned ? (long)uint.MaxValue : int.MaxValue;
        if (value != Math.Truncate(value) || value < minimum || value > maximum)
        {
            return null;
        }
        long integer = (long)value;
        return new FieldValue(integer.ToString(CultureInfo.InvariantCulture), integer);
    }

    // +0과 -0을 같은 값으로 비교하고 동일한 CSV로 기록한다.
    private static FieldValue FormatFloat32(float number) =>
        new(number == 0 ? "0" : number.ToString("R", CultureInfo.InvariantCulture), number);

    private static bool IsReferenceSyntax(string value)
    {
        string[] parts = value.Split('.');
        return parts.Length == 3 && !string.IsNullOrWhiteSpace(parts[0]) && parts[0] == parts[0].Trim() &&
            parts[0].IndexOfAny(['/', '\\', ':', '*', '?', '"', '<', '>', '|']) < 0 && !parts[0].Any(char.IsControl) &&
            WorkbookParser.IsIdentifier(parts[1]) && WorkbookParser.IsIdentifier(parts[2]);
    }
}
