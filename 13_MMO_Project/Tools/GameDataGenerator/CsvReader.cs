using System.Text;

internal static class CsvReader
{
    internal static readonly UTF8Encoding Utf8 = new(encoderShouldEmitUTF8Identifier: false, throwOnInvalidBytes: true);

    // 레코드의 CRLF와 인용된 셀 내부의 개행을 구분한다.
    public static List<string[]> Read(string path)
    {
        string source = Path.GetFileName(path);
        string text;
        try
        {
            text = Utf8.GetString(File.ReadAllBytes(path));
        }
        catch (DecoderFallbackException exception)
        {
            throw new InvalidDataException($"{source}: Invalid UTF-8.", exception);
        }
        if (text.Length != 0 && text[0] == '\uFEFF')
        {
            throw new InvalidDataException($"{source}: A UTF-8 BOM is not allowed.");
        }

        var rows = new List<string[]>();
        int index = 0;
        while (index < text.Length)
        {
            var fields = new List<string>();
            InvalidDataException Error(string reason) => new($"{source}: CSV record {rows.Count + 1}, column {fields.Count + 1}: {reason}");
            while (true)
            {
                if (index < text.Length && text[index] == '"')
                {
                    var field = new StringBuilder();
                    ++index;
                    while (true)
                    {
                        if (index == text.Length)
                        {
                            throw Error("Unterminated quoted field.");
                        }
                        char value = text[index++];
                        if (value == '"')
                        {
                            if (index == text.Length || text[index] != '"')
                            {
                                break;
                            }
                            ++index;
                        }
                        field.Append(value);
                    }
                    if (index < text.Length && text[index] is not (',' or '\r'))
                    {
                        throw Error("Unexpected character after quoted field.");
                    }
                    fields.Add(field.ToString());
                }
                else
                {
                    int start = index;
                    while (index < text.Length && text[index] is not (',' or '\r' or '\n'))
                    {
                        if (text[index] == '"')
                        {
                            throw Error("Unexpected quote in unquoted field.");
                        }
                        ++index;
                    }
                    fields.Add(text[start..index]);
                }
                if (index == text.Length)
                {
                    break;
                }
                if (text[index] == ',')
                {
                    ++index;
                    continue;
                }
                if (text[index] != '\r' || index + 1 == text.Length || text[index + 1] != '\n')
                {
                    throw Error("CSV records must be separated by CRLF.");
                }
                index += 2;
                break;
            }
            rows.Add(fields.ToArray());
        }
        return rows;
    }
}
