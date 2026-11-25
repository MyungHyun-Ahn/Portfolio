using System.Globalization;
using ClosedXML.Excel;

// double은 검증된 32비트 정수와 float32를 손실 없이 비교한다. 문자열과 Enum은 Text만 사용한다.
internal readonly record struct FieldValue(string Text, double Number = 0);

internal sealed record FieldDeclaration(string Type, bool IsPrimaryKey, bool IsRequired,
    FieldValue? Minimum, FieldValue? Maximum, HashSet<string>? Allowed, FieldValue? DefaultValue, string Reference,
    IReadOnlyDictionary<string, uint>? EnumMembers, string? ElementType)
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

        if (ElementType is string elementType)
        {
            return ReadArrayValue(cell, text, elementType, $"{source}!{cell.Address}:");
        }

        bool isText = Type == "string" || EnumMembers != null;
        if (cell.DataType != (isText ? XLDataType.Text : XLDataType.Number))
        {
            throw Error($"{Type} requires a {(isText ? "text" : "numeric")} Excel cell.");
        }

        if (EnumMembers != null && !EnumMembers.ContainsKey(text))
        {
            throw Error($"Unknown enum member '{text}' for {Type}.");
        }
        FieldValue? value = EnumMembers != null ? new FieldValue(text) :
            isText ? FieldDeclarations.ReadValue(Type, text) : FieldDeclarations.ReadNumber(Type, cell.GetDouble());
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

    // 데이터와 #Default에 같은 배열 문법과 원소 제약을 적용한다.
    public FieldValue ReadArrayValue(IXLCell cell, string text, string elementType, string location)
    {
        if (cell.DataType != XLDataType.Text)
        {
            throw new InvalidDataException($"{location} {Type} requires a text Excel cell.");
        }
        bool isString = elementType == "string";
        IReadOnlyList<string>? items = ArrayValues.ReadItems(text, isString);
        if (items == null)
        {
            throw new InvalidDataException($"{location} Invalid array syntax for {Type}.");
        }

        var formattedItems = new string[items.Count];
        for (int index = 0; index < items.Count; ++index)
        {
            string item = items[index];
            if (EnumMembers != null && !EnumMembers.ContainsKey(item))
            {
                throw new InvalidDataException($"{location} Array element {index + 1}: Unknown enum member '{item}' for {Type}.");
            }
            FieldValue? value = EnumMembers != null ? new FieldValue(item) : FieldDeclarations.ReadValue(elementType, item);
            if (value is not FieldValue parsed)
            {
                throw new InvalidDataException($"{location} Array element {index + 1} must be a valid {elementType}.");
            }
            if (GetConstraintError(parsed) is string reason)
            {
                throw new InvalidDataException($"{location} Array element {index + 1}: {reason}");
            }
            formattedItems[index] = isString ? ArrayValues.Quote(parsed.Text) : parsed.Text;
        }
        return new FieldValue("[" + string.Join(" | ", formattedItems) + "]");
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
        int column, bool hasPrimaryKey, EnumDefinitions enumDefinitions)
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
        bool isArray = type.EndsWith("[]", StringComparison.Ordinal);
        string scalarType = isArray ? type[..^2] : type;
        IReadOnlyDictionary<string, uint>? enumMembers = null;
        if (scalarType.StartsWith("enum:", StringComparison.Ordinal))
        {
            string enumName = scalarType[5..];
            enumMembers = enumDefinitions.Find(enumName) ?? throw Error(6, $"Unknown enum type '{enumName}' in Enum.xlsx.");
        }
        else if (scalarType is not ("int32" or "uint32" or "float32" or "string"))
        {
            throw Error(6, "must be int32, uint32, float32, string or enum:TypeName, optionally followed by one [] for an array type.");
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

        bool isText = scalarType == "string" || enumMembers != null;
        FieldValue? ReadMetadataValue(int row, string text)
        {
            IXLCell cell = worksheet.Cell(row, column);
            if (isText && cell.DataType != XLDataType.Text)
            {
                throw Error(row, "requires a text Excel cell.");
            }
            if (enumMembers != null)
            {
                return enumMembers.ContainsKey(text) ? new FieldValue(text) :
                    throw Error(row, $"Unknown enum member '{text}' for {type}.");
            }
            if (scalarType == "float32" && cell.DataType == XLDataType.Number)
            {
                FieldValue? value = ReadNumber(scalarType, cell.GetDouble());
                if (value is FieldValue parsed)
                {
                    values[(row, column)] = parsed.Text;
                }
                return value;
            }
            return ReadValue(scalarType, text);
        }

        FieldValue? ReadOptionalValue(int row)
        {
            string text = Text(row);
            return text.Length == 0 ? null : ReadMetadataValue(row, text) ?? throw Error(row, $"must be a valid {type}.");
        }

        if (isText && (Text(8).Length != 0 || Text(9).Length != 0))
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
        FieldValue? defaultValue = isArray ? null : ReadOptionalValue(DefaultRow);

        HashSet<string>? allowed = null;
        if (Text(11).Length != 0)
        {
            allowed = new HashSet<string>(StringComparer.Ordinal);
            StringSplitOptions options = type == "string" ? StringSplitOptions.None : StringSplitOptions.TrimEntries;
            IReadOnlyList<string> items = isArray && scalarType == "string" ?
                ArrayValues.ReadQuotedItems(Text(11)) ?? throw Error(11, "requires quoted string elements separated by '|'.") :
                Text(11).Split('|', options);
            foreach (string item in items)
            {
                if (ReadMetadataValue(11, item) is not FieldValue value || !allowed.Add(value.Text))
                {
                    throw Error(11, $"contains an invalid or duplicate {type} value.");
                }
            }
        }

        string reference = Text(ReferenceRow);
        if (isArray && reference.Length != 0)
        {
            throw Error(ReferenceRow, "is not valid for an array field.");
        }
        if (reference.Length != 0 && !IsReferenceSyntax(reference))
        {
            throw Error(ReferenceRow, "must use Workbook.Sheet.Field with no file extension.");
        }

        var declaration = new FieldDeclaration(type, key == "Primary", required == "true", minimum, maximum, allowed,
            defaultValue, reference, enumMembers, isArray ? scalarType : null);
        if (isArray && Text(DefaultRow).Length != 0)
        {
            IXLCell cell = worksheet.Cell(DefaultRow, column);
            FieldValue arrayDefault = declaration.ReadArrayValue(cell, Text(DefaultRow), scalarType, $"{source}!{cell.Address}: #Default");
            values[(DefaultRow, column)] = arrayDefault.Text;
            return declaration with { DefaultValue = arrayDefault };
        }
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
