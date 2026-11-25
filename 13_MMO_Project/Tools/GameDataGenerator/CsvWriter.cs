using System.Text;

internal static class CsvWriter
{
    private static readonly UTF8Encoding Utf8 = new(encoderShouldEmitUTF8Identifier: false, throwOnInvalidBytes: true);
    private static readonly char[] QuoteTriggers = [',', '"', '\r', '\n'];

    // 이번에 변환한 CSV만 함께 게시한다.
    public static void WriteDirectory(string csvDirectory, IReadOnlyList<CsvTable> tables)
    {
        using var batch = new OutputBatch(csvDirectory);
        foreach (CsvTable table in tables)
        {
            using var memory = new MemoryStream();
            using (var writer = new StreamWriter(memory, Utf8, leaveOpen: true))
            {
                writer.NewLine = "\r\n";
                foreach (string[] row in table.Rows)
                {
                    for (int column = 0; column < row.Length; ++column)
                    {
                        if (column != 0)
                        {
                            writer.Write(',');
                        }
                        WriteField(writer, row[column]);
                    }
                    writer.WriteLine();
                }
            }

            batch.Stage(Path.Combine(csvDirectory, table.Name + ".csv"), memory.ToArray());
        }
        batch.Commit();
    }

    private static void WriteField(TextWriter writer, string value)
    {
        if (value.IndexOfAny(QuoteTriggers) < 0)
        {
            writer.Write(value);
            return;
        }

        writer.Write('"');
        writer.Write(value.Replace("\"", "\"\"", StringComparison.Ordinal));
        writer.Write('"');
    }
}
