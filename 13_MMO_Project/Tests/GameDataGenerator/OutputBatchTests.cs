using System.Reflection;
using System.Text;

string root = args[0];
string output = Path.Combine(root, "Generated");
Directory.CreateDirectory(output);
string server = Path.Combine(output, "Test.sdat");
string client = Path.Combine(output, "Test.cdat");
string manual = Path.Combine(root, "Tables", "Client", "CTestTable.h");
File.WriteAllBytes(server, [1]);
File.WriteAllBytes(client, [2]);

// 후보의 누락과 같은 길이의 바이트 변조를 각각 재현한다.
foreach (bool missing in new[] { true, false })
{
    using (var batch = new OutputBatch(output))
    {
        batch.Stage(server, [3]);
        string candidate = batch.Stage(client, [4]);
        batch.Stage(manual, [5], createOnly: true);
        if (missing)
        {
            File.Delete(candidate);
        }
        else
        {
            File.WriteAllBytes(candidate, [9]);
        }
        try
        {
            batch.Commit();
            throw new Exception("Invalid candidate was published.");
        }
        catch (InvalidDataException exception) when (exception.Message.Contains(client, StringComparison.Ordinal))
        {
        }
    }
    CheckOriginalOutputs();
}

if (OperatingSystem.IsWindows())
{
    // 검사는 통과하지만 교체 직전에 후보의 이름 변경이 막힌다.
    using (var batch = new OutputBatch(output))
    {
        batch.Stage(server, [3]);
        batch.Stage(manual, [5], createOnly: true);
        string candidate = batch.Stage(client, [4]);
        using var lockedCandidate = File.Open(candidate, FileMode.Open, FileAccess.Read, FileShare.Read);
        try
        {
            batch.Commit();
            throw new Exception("Locked candidate was published.");
        }
        catch (IOException exception) when (exception.Message.StartsWith($"Could not publish '{client}'", StringComparison.Ordinal))
        {
        }
    }
    CheckOriginalOutputs();

    // 새 파일 삭제와 진단 쓰기가 모두 실패해도 앞서 옮긴 원본은 복구한다.
    using var rollbackWriter = new FailedWriter(new UnauthorizedAccessException("Injected console access denial."));
    TextWriter priorError = Console.Error;
    using (var batch = new OutputBatch(output))
    {
        batch.Stage(server, [3]);
        string readOnlyCandidate = batch.Stage(manual, [5], createOnly: true);
        File.SetAttributes(readOnlyCandidate, FileAttributes.ReadOnly);
        string candidate = batch.Stage(client, [4]);
        using var lockedCandidate = File.Open(candidate, FileMode.Open, FileAccess.Read, FileShare.Read);
        Console.SetError(rollbackWriter);
        try
        {
            batch.Commit();
            throw new Exception("Locked candidate was published.");
        }
        catch (IOException exception) when (exception.Message.StartsWith($"Could not publish '{client}'", StringComparison.Ordinal))
        {
        }
        finally
        {
            Console.SetError(priorError);
        }
    }
    if (rollbackWriter.Attempts == 0 || !File.Exists(manual))
    {
        throw new Exception("Rollback did not reach the failed delete and diagnostic.");
    }
    File.SetAttributes(manual, FileAttributes.Normal);
    Directory.Delete(Path.Combine(root, "Tables"), recursive: true);
    CheckOriginalOutputs();
}

TestFailedConsoleOutput(args[1]);
TestInvalidArtifactRoot();

using (var batch = new OutputBatch(output))
{
    batch.Stage(server, [3]);
    batch.Stage(client, [4]);
    batch.Stage(manual, [5], createOnly: true);
    batch.Commit();
}
if (!File.ReadAllBytes(server).AsSpan().SequenceEqual(new byte[] { 3 }) ||
    !File.ReadAllBytes(client).AsSpan().SequenceEqual(new byte[] { 4 }) ||
    !File.ReadAllBytes(manual).AsSpan().SequenceEqual(new byte[] { 5 }) ||
    Directory.EnumerateFileSystemEntries(root, ".game-data-output-*").Any())
{
    throw new Exception("A normal batch was not fully published after failures.");
}

void CheckOriginalOutputs()
{
    if (!File.ReadAllBytes(server).AsSpan().SequenceEqual(new byte[] { 1 }) ||
        !File.ReadAllBytes(client).AsSpan().SequenceEqual(new byte[] { 2 }) ||
        Directory.Exists(Path.Combine(root, "Tables")) ||
        Directory.EnumerateFileSystemEntries(root, ".game-data-output-*").Any())
    {
        throw new Exception("A rejected batch changed the original output or left staging files.");
    }
}

// 실제 CLI를 호출하되, 결과와 오류의 콘솔 쓰기만 실패시킨다.
void TestFailedConsoleOutput(string workbook)
{
    string excel = Path.Combine(root, "Excel");
    string csv = Path.Combine(root, "ConsoleCsv");
    string generated = Path.Combine(root, "ConsoleGenerated");
    Directory.CreateDirectory(excel);
    File.Copy(workbook, Path.Combine(excel, "Player.xlsx"));
    MethodInfo main = typeof(OutputBatch).Assembly.EntryPoint!;
    TextWriter priorOut = Console.Out;
    TextWriter priorError = Console.Error;
    using var failedWriter = new FailedWriter(new UnauthorizedAccessException("Injected console access denial."));
    int excelResult;
    int emitResult;
    int failedResult;
    Console.SetOut(failedWriter);
    Console.SetError(failedWriter);
    try
    {
        excelResult = (int)main.Invoke(null, [new[] { excel, csv }])!;
        emitResult = (int)main.Invoke(null, [new[] { "--emit", csv, generated, "PlayerMovement" }])!;
        failedResult = (int)main.Invoke(null, [new[] { "--emit", csv, generated, "Missing" }])!;
    }
    finally
    {
        Console.SetOut(priorOut);
        Console.SetError(priorError);
    }
    if (excelResult != 0 || emitResult != 0 || failedResult != 1 || failedWriter.Attempts < 3 ||
        !File.Exists(Path.Combine(csv, "PlayerMovement.csv")) ||
        !File.Exists(Path.Combine(generated, "Data", "Client", "Player", "PlayerMovement.cdat")))
    {
        throw new Exception("Console failure changed a CLI result or prevented publication.");
    }
}

// 현재 선택에서 빠진 산출물 경로도 형식이 잘못되면 부재로 처리하지 않는다.
void TestInvalidArtifactRoot()
{
    string csv = Path.Combine(root, "ConsoleCsv");
    string input = Path.Combine(csv, "PlayerMovement.csv");
    File.WriteAllText(input, File.ReadAllText(input).Replace("#Target,Shared,", "#Target,Server,", StringComparison.Ordinal));
    string generated = Path.Combine(root, "InvalidGenerated");
    string data = Path.Combine(generated, "Data");
    Directory.CreateDirectory(data);
    string invalidRoot = Path.Combine(data, "Client");
    File.WriteAllBytes(invalidRoot, [1]);
    string error;
    try
    {
        GameDataArtifacts.Generate(csv, generated, ["PlayerMovement"]);
        error = "";
    }
    catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
    {
        error = exception.Message;
    }
    if (!error.Contains(invalidRoot, StringComparison.Ordinal) || File.Exists(Path.Combine(generated, "Server", "GameDataAll.g.h")) ||
        !File.ReadAllBytes(invalidRoot).AsSpan().SequenceEqual(new byte[] { 1 }))
    {
        throw new Exception($"Invalid artifact root did not block publication: '{error}'.");
    }
}

sealed class FailedWriter(Exception failure) : TextWriter
{
    public int Attempts { get; private set; }
    public override Encoding Encoding => Encoding.UTF8;

    public override void WriteLine(string? value)
    {
        ++Attempts;
        throw failure;
    }
}