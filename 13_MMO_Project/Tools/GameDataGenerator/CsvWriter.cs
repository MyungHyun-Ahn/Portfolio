using System.Text;

internal static class CsvWriter
{
    private static readonly UTF8Encoding Utf8 = new(encoderShouldEmitUTF8Identifier: false, throwOnInvalidBytes: true);
    private static readonly char[] QuoteTriggers = [',', '"', '\r', '\n'];

    // 검증된 시트를 모두 임시 위치에 쓴 뒤 대상 CSV만 교체한다.
    public static void WriteDirectory(string csvDirectory, IReadOnlyList<CsvTable> tables)
    {
        Directory.CreateDirectory(csvDirectory);
        string candidateDirectory = Path.Combine(csvDirectory, $".game-data-csv-{Guid.NewGuid():N}");
        Directory.CreateDirectory(candidateDirectory);
        try
        {
            foreach (CsvTable table in tables)
            {
                string fileName = table.Name + ".csv";
                if (Directory.Exists(Path.Combine(csvDirectory, fileName)))
                {
                    throw new IOException($"The CSV destination is a directory: {fileName}.");
                }

                using var writer = new StreamWriter(Path.Combine(candidateDirectory, fileName), append: false, Utf8);
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

            foreach (CsvTable table in tables)
            {
                string fileName = table.Name + ".csv";
                File.Move(Path.Combine(candidateDirectory, fileName), Path.Combine(csvDirectory, fileName), overwrite: true);
            }
        }
        finally
        {
            try
            {
                Directory.Delete(candidateDirectory, recursive: true);
            }
            catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
            {
                // 정리 실패는 원래 오류나 이미 완료된 교체 결과를 가리지 않는다.
                Console.Error.WriteLine($"Warning: Could not clean '{candidateDirectory}': {exception.Message}");
            }
        }
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
