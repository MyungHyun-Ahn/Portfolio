internal static class Program
{
    private static int Main(string[] args)
    {
        bool isEmit = args.Length != 0 && args[0] == "--emit";
        if (isEmit ? args.Length < 4 : args.Length is not (0 or 2))
        {
            Console.Error.WriteLine("Usage: GameDataGenerator [excel-directory csv-directory]");
            Console.Error.WriteLine("       GameDataGenerator --emit csv-directory output-directory table-name [table-name ...]");
            return 1;
        }

        try
        {
            if (isEmit)
            {
                GameDataArtifacts.Generate(Path.GetFullPath(args[1]), Path.GetFullPath(args[2]), args[3..]);
                return 0;
            }

            string excelDirectory;
            string csvDirectory;
            if (args.Length == 0)
            {
                string gameDataDirectory = FindGameDataDirectory();
                excelDirectory = Path.Combine(gameDataDirectory, "Excel");
                csvDirectory = Path.Combine(gameDataDirectory, "Csv");
            }
            else
            {
                excelDirectory = Path.GetFullPath(args[0]);
                csvDirectory = Path.GetFullPath(args[1]);
            }

            IReadOnlyList<CsvTable> tables = WorkbookParser.ReadDirectory(excelDirectory);
            CsvWriter.WriteDirectory(csvDirectory, tables);
            foreach (CsvTable table in tables)
            {
                Console.WriteLine(Path.Combine(csvDirectory, table.Name + ".csv"));
            }

            return 0;
        }
        catch (Exception exception)
        {
            Console.Error.WriteLine(exception.Message);
            return 1;
        }
    }

    // 실행 위치가 달라도 현재 도구 솔루션의 GameData를 찾는다.
    private static string FindGameDataDirectory()
    {
        for (DirectoryInfo? directory = new(AppContext.BaseDirectory); directory != null; directory = directory.Parent)
        {
            if (File.Exists(Path.Combine(directory.FullName, "Tools.sln")) && directory.Parent != null)
            {
                return Path.Combine(directory.Parent.FullName, "GameData");
            }
        }

        throw new DirectoryNotFoundException("Tools.sln was not found. Pass excel-directory and csv-directory explicitly.");
    }
}
