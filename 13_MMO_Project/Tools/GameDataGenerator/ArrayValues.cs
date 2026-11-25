using System.Text;

internal static class ArrayValues
{
    // 셀의 바깥 괄호를 벗기고 원소 순서대로 읽는다.
    public static IReadOnlyList<string>? ReadItems(string value, bool isString)
    {
        ReadOnlySpan<char> text = value.AsSpan().Trim();
        if (text.Length < 2 || text[0] != '[' || text[^1] != ']')
        {
            return null;
        }
        text = text[1..^1].Trim();
        if (text.IsEmpty)
        {
            return Array.Empty<string>();
        }
        return isString ? ReadQuotedItems(text) : text.ToString().Split('|', StringSplitOptions.TrimEntries);
    }

    // 큰따옴표 안의 구분자와 공백을 보존하고 겹쳐 쓴 큰따옴표만 복원한다.
    public static List<string>? ReadQuotedItems(ReadOnlySpan<char> text)
    {
        var items = new List<string>();
        text = text.TrimStart();
        while (!text.IsEmpty && text[0] == '"')
        {
            var item = new StringBuilder();
            int end = 1;
            for (; end < text.Length; ++end)
            {
                if (text[end] == '"')
                {
                    if (end + 1 == text.Length || text[end + 1] != '"')
                    {
                        break;
                    }
                    ++end;
                }
                item.Append(text[end]);
            }
            if (end == text.Length)
            {
                return null;
            }
            items.Add(item.ToString());
            text = text[(end + 1)..].TrimStart();
            if (text.IsEmpty)
            {
                return items;
            }
            if (text[0] != '|')
            {
                return null;
            }
            text = text[1..].TrimStart();
        }
        return null;
    }

    public static string Quote(string value) => '"' + value.Replace("\"", "\"\"", StringComparison.Ordinal) + '"';
}
