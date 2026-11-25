internal sealed class OutputBatch : IDisposable
{
    private sealed record Entry(string Destination, string Candidate, string Backup, byte[] Bytes, bool CreateOnly);

    private readonly string parent;
    private readonly string staging;
    private readonly List<Entry> entries = [];
    private bool keepBackup;

    public OutputBatch(string outputDirectory)
    {
        parent = Path.GetDirectoryName(Path.TrimEndingDirectorySeparator(outputDirectory)) ?? outputDirectory;
        while (!Directory.Exists(parent))
        {
            parent = Path.GetDirectoryName(parent) ?? throw new DirectoryNotFoundException($"No existing parent for '{outputDirectory}'.");
        }
        staging = Path.Combine(parent, $".game-data-output-{Guid.NewGuid():N}");
        Directory.CreateDirectory(staging);
    }

    public string Stage(string destination, byte[] bytes, bool createOnly = false)
    {
        string candidate = Path.Combine(staging, $"{entries.Count}.new");
        File.WriteAllBytes(candidate, bytes);
        entries.Add(new Entry(destination, candidate, Path.Combine(staging, $"{entries.Count}.old"), bytes, createOnly));
        return candidate;
    }

    // 모든 후보를 다시 읽은 뒤에만 실제 파일과 디렉터리를 바꾼다.
    public void Commit()
    {
        foreach (Entry entry in entries)
        {
            if (!File.Exists(entry.Candidate) || !File.ReadAllBytes(entry.Candidate).AsSpan().SequenceEqual(entry.Bytes))
            {
                throw new InvalidDataException($"Missing or invalid staged output for '{entry.Destination}'.");
            }
            ValidateDestination(entry.Destination);
        }

        var published = new List<(Entry File, bool HasBackup)>(entries.Count);
        var createdDirectories = new List<string>();
        foreach (Entry entry in entries)
        {
            try
            {
                if (entry.CreateOnly && File.Exists(entry.Destination))
                {
                    continue;
                }
                CreateDestinationDirectory(Path.GetDirectoryName(entry.Destination)!, createdDirectories);
                bool hasBackup = !entry.CreateOnly && File.Exists(entry.Destination);
                if (hasBackup)
                {
                    File.Move(entry.Destination, entry.Backup);
                    published.Add((entry, true));
                }
                try
                {
                    File.Move(entry.Candidate, entry.Destination);
                    if (!hasBackup)
                    {
                        published.Add((entry, false));
                    }
                }
                catch (IOException) when (entry.CreateOnly && File.Exists(entry.Destination))
                {
                    // 먼저 생긴 수기 파일은 건드리지 않는다.
                }
            }
            catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
            {
                keepBackup = true;
                keepBackup = !RollBack(published, createdDirectories);
                throw new IOException($"Could not publish '{entry.Destination}': {exception.Message}", exception);
            }
        }
        foreach ((Entry entry, _) in published)
        {
            if (entry.CreateOnly)
            {
                Program.WriteDiagnostic(Console.Out, entry.Destination);
            }
        }
    }

    // 임시 파일과 대상이 서로 다른 볼륨으로 우회되지 않도록 검사한다.
    private void ValidateDestination(string destination)
    {
        if (Directory.Exists(destination))
        {
            throw new IOException($"Invalid output destination: '{destination}'.");
        }
        for (string? directory = Path.GetDirectoryName(destination); directory != parent; directory = Path.GetDirectoryName(directory))
        {
            if (directory == null || File.Exists(directory) ||
                (Directory.Exists(directory) && (File.GetAttributes(directory) & FileAttributes.ReparsePoint) != 0))
            {
                throw new IOException($"Invalid output destination: '{destination}'.");
            }
        }
    }

    private static void CreateDestinationDirectory(string directory, List<string> createdDirectories)
    {
        if (Directory.Exists(directory))
        {
            return;
        }
        CreateDestinationDirectory(Path.GetDirectoryName(directory)!, createdDirectories);
        Directory.CreateDirectory(directory);
        createdDirectories.Add(directory);
    }

    // 복구도 막히면 원본 백업을 지우지 않고 정확한 복구 위치를 남긴다.
    private static bool RollBack(List<(Entry File, bool HasBackup)> published, List<string> createdDirectories)
    {
        bool hasUnrestoredBackup = false;
        for (int index = published.Count - 1; index >= 0; --index)
        {
            (Entry entry, bool hasBackup) = published[index];
            try
            {
                if (hasBackup)
                {
                    File.Move(entry.Backup, entry.Destination, overwrite: true);
                }
                else
                {
                    File.Delete(entry.Destination);
                }
            }
            catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
            {
                hasUnrestoredBackup |= hasBackup;
                string recovery = hasBackup ? $" Old file retained at '{entry.Backup}'." : "";
                Program.WriteDiagnostic(Console.Error, $"Rollback failed for '{entry.Destination}': {exception.Message}.{recovery}");
            }
        }
        for (int index = createdDirectories.Count - 1; index >= 0; --index)
        {
            string directory = createdDirectories[index];
            try
            {
                Directory.Delete(directory);
            }
            catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
            {
                Program.WriteDiagnostic(Console.Error, $"Rollback could not remove '{directory}': {exception.Message}");
            }
        }
        return !hasUnrestoredBackup;
    }

    public void Dispose()
    {
        if (keepBackup)
        {
            return;
        }
        try
        {
            Directory.Delete(staging, recursive: true);
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
        {
            Program.WriteDiagnostic(Console.Error, $"Warning: Could not clean '{staging}': {exception.Message}");
        }
    }
}