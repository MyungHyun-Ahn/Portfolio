using System.Globalization;
using System.Security.Cryptography;
using System.Text;

internal static class GameDataArtifacts
{
    private const ushort FormatVersion = 1;

    // 인자로 선택한 CSV만 읽는다. 삭제된 시트의 이전 CSV는 자동으로 가져오지 않는다.
    public static void Generate(string csvDirectory, string outputDirectory, string[] names)
    {
        var selected = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (string name in names)
        {
            if (!EnumDefinitions.IsCppName(name) || !WorkbookParser.IsTableName(name) ||
                (string.Equals(name, EnumDefinitions.CsvName, StringComparison.OrdinalIgnoreCase) && name != EnumDefinitions.CsvName))
            {
                throw new InvalidDataException($"Invalid CSV name '{name}'. Use a table name without the .csv extension.");
            }
            if (!selected.Add(name))
            {
                throw new InvalidDataException($"Duplicate selected CSV name '{name}'.");
            }
        }
        var enumDefinitions = new EnumDefinitions();
        if (selected.Remove(EnumDefinitions.CsvName))
        {
            enumDefinitions.ReadCsv(CsvReader.Read(Path.Combine(csvDirectory, "Enum.csv")));
        }
        var tables = new Dictionary<string, DataTable>(StringComparer.Ordinal);
        var workbookNames = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        foreach (string name in selected.Order(StringComparer.Ordinal))
        {
            DataTable table = CsvDataTables.Read(csvDirectory, name, enumDefinitions);
            if (workbookNames.TryGetValue(table.WorkbookName, out string? prior) && prior != table.WorkbookName)
            {
                throw new InvalidDataException($"{name}.csv!B{WorkbookParser.WorkbookRow}: #Workbook '{table.WorkbookName}' conflicts with '{prior}'.");
            }
            workbookNames[table.WorkbookName] = table.WorkbookName;
            tables.Add(name, table);
        }
        CsvDataTables.ValidateReferences(tables);

        var artifacts = new Dictionary<string, byte[]>(StringComparer.OrdinalIgnoreCase)
        {
            [Path.Combine("Common", "GameDataEnums.h")] = WriteEnums(enumDefinitions),
        };
        var manualTables = new Dictionary<string, byte[]>(StringComparer.OrdinalIgnoreCase);
        foreach (DataTable table in tables.Values)
        {
            AddProjection(table, "Server", 1, ".sdat", artifacts, manualTables);
            AddProjection(table, "Client", 2, ".cdat", artifacts, manualTables);
        }
        AddTableBundle(tables.Values, "Server", ".sdat", artifacts);
        AddTableBundle(tables.Values, "Client", ".cdat", artifacts);
        Publish(outputDirectory, artifacts, manualTables);
    }

    // 같은 필드 목록과 스키마 식별자를 헤더와 바이너리에서 사용한다.
    private static void AddProjection(DataTable table, string target, byte targetId, string extension,
        Dictionary<string, byte[]> artifacts, Dictionary<string, byte[]> manualTables)
    {
        if (!table.IncludesTarget(target))
        {
            return;
        }
        int[] projection = Enumerable.Range(0, table.Columns.Length)
            .Where(index => CsvDataTables.IsProjected(table, table.Columns[index], target)).ToArray();
        byte[] schemaHash = WriteSchemaHash(table, projection);
        artifacts.Add(Path.Combine(target, table.WorkbookName, table.Name + ".h"), WriteHeader(table, projection, target, targetId, schemaHash));
        artifacts.Add(Path.Combine(target, table.WorkbookName, table.Name + ".TableBase.g.h"), WriteTableBase(table, projection, target));
        artifacts.Add(Path.Combine("Data", target, table.WorkbookName, table.Name + extension), WriteBinary(table, projection, targetId, schemaHash));
        manualTables.Add(Path.Combine(target, $"C{table.Name}Table.h"), WriteManualTable(table, target));
    }

    // 생성 Load는 투영된 필드를 공통 판독기에 연결한다.
    private static byte[] WriteTableBase(DataTable table, int[] projection, string target)
    {
        var text = new StringBuilder("#pragma once\n\n");
        string row = $"S{table.Name}Row";
        string tableBase = $"C{table.Name}TableBase";
        string format = $"{table.Name}Format";
        string key = table.Columns[table.KeyIndex].MemberName;
        text.Append($"namespace GameData::{target}::{table.WorkbookName}\n{{\n\tclass {tableBase} : public IGameDataTable\n\t{{\n\tpublic:\n");
        text.Append("\t\t[[nodiscard]]\n\t\tbool Load(const std::filesystem::path& path) final\n\t\t{\n");
        text.Append($"\t\t\tstatic_assert({format}::kFormatVersion == CGameDataReader::kSupportedFormatVersion);\n");
        text.Append("\t\t\tconst SGameDataFormat format\n\t\t\t{\n");
        foreach (string field in new[] { "kFormatVersion", "kTarget", "kTableName", "kSchemaHash" })
        {
            text.Append($"\t\t\t\t{format}::{field},\n");
        }
        text.Append("\t\t\t};\n");
        text.Append($"\t\t\tm_lastReadResult = CGameDataReader::Read(path, format, &readRow, &{row}::{key}, m_rows);\n");
        text.Append("\t\t\treturn m_lastReadResult == EGameDataReadResult::Success;\n\t\t}\n\n");
        text.Append("\t\t[[nodiscard]]\n\t\tEGameDataReadResult GetLastReadResult() const noexcept\n\t\t{\n\t\t\treturn m_lastReadResult;\n\t\t}\n\n");
        text.Append("\t\t// 현재 묶음에서 PK에 해당하는 읽기 전용 행을 찾는다.\n");
        text.Append($"\t\t[[nodiscard]]\n\t\tstatic const {row}* Find(std::uint32_t id);\n\n");
        text.Append("\tprotected:\n\t\t[[nodiscard]]\n\t\tconst auto& GetRows() const noexcept\n\t\t{\n\t\t\treturn m_rows;\n\t\t}\n\n");
        text.Append($"\tprivate:\n\t\t[[nodiscard]]\n\t\tstatic bool readRow(CGameDataReader& reader, {row}& outRow)\n\t\t{{\n");
        foreach (int index in projection)
        {
            text.Append($"\t\t\tif (!reader.ReadValue(outRow.{table.Columns[index].MemberName}))\n\t\t\t{{\n\t\t\t\treturn false;\n\t\t\t}}\n");
        }
        text.Append("\t\t\treturn true;\n\t\t}\n\n\tprivate:\n");
        text.Append($"\t\tstd::unordered_map<std::uint32_t, {row}> m_rows;\n");
        text.Append("\t\tEGameDataReadResult m_lastReadResult = EGameDataReadResult::NotRead;\n\t};\n}\n");
        return CsvReader.Utf8.GetBytes(text.ToString());
    }

    private static byte[] WriteManualTable(DataTable table, string target)
    {
        string name = table.Name;
        string text = $"#pragma once\n\nnamespace GameData::{target}::{table.WorkbookName}\n{{\n\tclass C{name}Table final : public C{name}TableBase\n\t{{\n\tpublic:\n" +
            "\t\t// 모든 테이블을 읽은 뒤 필요한 파생 데이터를 준비한다.\n\t\t[[nodiscard]]\n\t\tbool Init() override\n" +
            "\t\t{\n\t\t\treturn true;\n\t\t}\n\n" +
            "\t\t// 테이블 규칙을 구현하기 전에는 사용하지 않는다.\n\t\t[[nodiscard]]\n\t\tbool Validate() const override\n" +
            "\t\t{\n\t\t\treturn false;\n\t\t}\n\t};\n}\n";
        return CsvReader.Utf8.GetBytes(text);
    }

    // 발행 대상과 같은 테이블만 묶고, 실행 시 받는 디렉터리 아래의 상대 경로만 생성한다.
    private static void AddTableBundle(IEnumerable<DataTable> tables, string target, string extension,
        Dictionary<string, byte[]> artifacts)
    {
        DataTable[] projected = tables.Where(table => table.IncludesTarget(target)).ToArray();
        if (projected.Length == 0)
        {
            return;
        }

        var includes = new StringBuilder("#pragma once\n\n#include \"GameData/IGameDataTable.h\"\n");
        includes.Append("#include \"GameData/CGameDataReader.h\"\n#include \"Common/GameDataEnums.h\"\n");
        foreach (DataTable table in projected)
        {
            includes.Append($"#include \"{target}/{table.WorkbookName}/{table.Name}.h\"\n");
            includes.Append($"#include \"{target}/{table.WorkbookName}/{table.Name}.TableBase.g.h\"\n");
        }
        foreach (DataTable table in projected)
        {
            includes.Append($"#include \"Tables/{target}/C{table.Name}Table.h\"\n");
        }
        includes.Append($"#include \"{target}/CGameDataTables.g.h\"\n");
        artifacts.Add(Path.Combine(target, "GameDataAll.g.h"), CsvReader.Utf8.GetBytes(includes.ToString()));

        var text = new StringBuilder("#pragma once\n\n");
        text.Append($"namespace GameData::{target}\n{{\n\tclass CGameDataTables\n\t{{\n");
        foreach (DataTable table in projected)
        {
            text.Append($"\t\tfriend class ::GameData::{target}::{table.WorkbookName}::C{table.Name}TableBase;\n");
        }
        text.Append("\n\tpublic:\n");
        text.Append("\t\t// 소유한 테이블이 사라지기 전에 정적 조회를 해제한다.\n\t\t~CGameDataTables()\n\t\t{\n");
        text.Append("\t\t\tm_hasLoadedTables = false;\n\t\t\tm_tables = {};\n\t\t\tsm_pTables = nullptr;\n\t\t}\n\n");
        text.Append("\t\t// 워커 시작 전에 전체 Load, 전체 Init, 전체 Validate를 순서대로 실행한다.\n\t\t[[nodiscard]]\n");
        text.Append("\t\tstatic std::unique_ptr<CGameDataTables> Load(const std::filesystem::path& dataDirectory)\n\t\t{\n");
        text.Append("\t\t\tusing ::MHLib::utils::CLogger;\n\t\t\tusing ::MHLib::utils::ELogLevel;\n\n");
        text.Append("\t\t\tauto* pLogger = CLogger::GetInstance();\n");
        text.Append("\t\t\tif (dataDirectory.empty() || sm_pTables != nullptr)\n\t\t\t{\n");
        text.Append("\t\t\t\tpLogger->WriteLog(ELogLevel::Error, L\"GameData\", 0, std::source_location::current(),\n");
        text.Append($"\t\t\t\t\tL\"event=game_data_bundle_rejected target={target} reason=%ls | 게임 데이터 로드를 시작할 수 없습니다.\",\n");
        text.Append("\t\t\t\t\tdataDirectory.empty() ? L\"empty_directory\" : L\"bundle_already_exists\");\n");
        text.Append("\t\t\t\treturn nullptr;\n\t\t\t}\n\n");
        text.Append("\t\t\tauto pTables = std::unique_ptr<CGameDataTables>(new CGameDataTables());\n\t\t\tsm_pTables = pTables.get();\n");
        text.Append("\t\t\tpTables->m_tables =\n\t\t\t{{\n");
        foreach (DataTable table in projected)
        {
            text.Append($"\t\t\t\t{{ \"{table.WorkbookName}/{table.Name}{extension}\",\n");
            text.Append($"\t\t\t\t\tstd::make_unique<::GameData::{target}::{table.WorkbookName}::C{table.Name}Table>() }},\n");
        }
        text.Append("\t\t\t}};\n\n");
        foreach ((string function, string stage, string action) in new[]
            { ("Load", "load", "로드"), ("Init", "init", "초기화"), ("Validate", "validate", "검증") })
        {
            text.Append("\t\t\tfor (const auto& table : pTables->m_tables)\n\t\t\t{\n");
            string argument = function == "Load" ? "dataDirectory / table.m_pRelativePath" : string.Empty;
            text.Append($"\t\t\t\tif (!table.m_pTable->{function}({argument}))\n\t\t\t\t{{\n");
            text.Append("\t\t\t\t\tpLogger->WriteLog(ELogLevel::Error, L\"GameData\", 0, std::source_location::current(),\n");
            text.Append($"\t\t\t\t\t\tL\"event=game_data_table_failed target={target} stage={stage} file=%hs | 게임 데이터 {action} 실패.\",\n");
            text.Append("\t\t\t\t\t\ttable.m_pRelativePath);\n\t\t\t\t\treturn nullptr;\n\t\t\t\t}\n");
            text.Append("\t\t\t\tpLogger->WriteLog(ELogLevel::Debug, L\"GameData\", 0, std::source_location::current(),\n");
            text.Append($"\t\t\t\t\tL\"event=game_data_table_completed target={target} stage={stage} file=%hs | 게임 데이터 {action} 완료.\",\n");
            text.Append("\t\t\t\t\ttable.m_pRelativePath);\n\t\t\t}\n");
            if (function == "Load")
            {
                text.Append("\t\t\tpTables->m_hasLoadedTables = true;\n");
            }
            text.Append("\t\t\tpLogger->WriteLog(ELogLevel::Info, L\"GameData\", 0, std::source_location::current(),\n");
            text.Append($"\t\t\t\tL\"event=game_data_stage_completed target={target} stage={stage} tables=%zu | 전체 게임 데이터 {action} 성공.\",\n");
            text.Append("\t\t\t\tpTables->m_tables.size());\n\n");
        }
        text.Append("\t\t\treturn pTables;\n\t\t}\n\n\tprivate:\n\t\tCGameDataTables() = default;\n\n");
        text.Append("\t\t// 전체 로드가 끝난 묶음에서만 생성된 배열 위치로 테이블을 찾는다.\n\t\t[[nodiscard]]\n");
        text.Append("\t\tstatic const IGameDataTable* getTable(std::size_t index) noexcept\n\t\t{\n");
        text.Append("\t\t\tif (sm_pTables == nullptr || !sm_pTables->m_hasLoadedTables)\n\t\t\t{\n\t\t\t\treturn nullptr;\n\t\t\t}\n");
        text.Append("\t\t\treturn sm_pTables->m_tables[index].m_pTable.get();\n\t\t}\n\n");
        text.Append("\tprivate:\n\t\tstruct STableEntry\n\t\t{\n\t\t\tconst char* m_pRelativePath;\n");
        text.Append("\t\t\tstd::unique_ptr<IGameDataTable> m_pTable;\n\t\t};\n\n");
        text.Append("\t\tinline static const CGameDataTables* sm_pTables = nullptr;\n");
        text.Append($"\t\tstd::array<STableEntry, {projected.Length}> m_tables{{}};\n\t\tbool m_hasLoadedTables = false;\n\t}};\n}}\n");

        // 등록 배열과 같은 순서로 Find의 슬롯을 만들어 서로 다른 타입이 섞이지 않게 한다.
        for (int index = 0; index < projected.Length; ++index)
        {
            DataTable table = projected[index];
            string tableBase = $"C{table.Name}TableBase";
            text.Append($"\nnamespace GameData::{target}::{table.WorkbookName}\n{{\n");
            text.Append($"\tinline const S{table.Name}Row* {tableBase}::Find(std::uint32_t id)\n\t{{\n");
            text.Append($"\t\tconst auto* pTable = static_cast<const {tableBase}*>(\n");
            text.Append($"\t\t\t::GameData::{target}::CGameDataTables::getTable({index}));\n");
            text.Append("\t\tif (pTable == nullptr)\n\t\t{\n\t\t\treturn nullptr;\n\t\t}\n");
            text.Append("\t\tconst auto iter = pTable->m_rows.find(id);\n");
            text.Append("\t\treturn iter == pTable->m_rows.end() ? nullptr : &iter->second;\n\t}\n}\n");
        }
        artifacts.Add(Path.Combine(target, "CGameDataTables.g.h"), CsvReader.Utf8.GetBytes(text.ToString()));
    }

    private static byte[] WriteSchemaHash(DataTable table, int[] projection)
    {
        var schema = new StringBuilder();
        var enums = new SortedDictionary<string, IReadOnlyDictionary<string, uint>>(StringComparer.Ordinal);
        foreach (int index in projection)
        {
            DataColumn column = table.Columns[index];
            FieldDeclaration field = column.Field;
            schema.Append(column.Name).Append(':').Append(field.Type).Append(':')
                .Append(field.IsRequired ? "required\n" : "optional\n");
            if (field.EnumMembers != null)
            {
                enums.TryAdd((field.ElementType ?? field.Type)[5..], field.EnumMembers);
            }
        }
        foreach ((string type, IReadOnlyDictionary<string, uint> members) in enums)
        {
            foreach ((string name, uint number) in members.OrderBy(pair => pair.Key, StringComparer.Ordinal))
            {
                schema.Append("enum:").Append(type).Append(':').Append(name).Append(':')
                    .Append(number.ToString(CultureInfo.InvariantCulture)).Append('\n');
            }
        }
        return SHA256.HashData(CsvReader.Utf8.GetBytes(schema.ToString()));
    }

    private static byte[] WriteHeader(DataTable table, int[] projection, string target, byte targetId, byte[] schemaHash)
    {
        var text = new StringBuilder("#pragma once\n\n");
        text.Append($"namespace GameData::{target}::{table.WorkbookName}\n{{\n");
        text.Append("\tstruct S").Append(table.Name).Append("Row\n\t{\n");
        var getters = new StringBuilder();
        foreach (int index in projection)
        {
            DataColumn column = table.Columns[index];
            FieldDeclaration field = column.Field;
            string scalarType = field.ElementType ?? field.Type;
            string cppType = scalarType switch
            {
                "int32" => "std::int32_t",
                "uint32" => "std::uint32_t",
                "float32" => "float",
                "string" => "std::string",
                _ => "::GameData::" + scalarType[5..],
            };
            if (field.ElementType != null)
            {
                cppType = $"std::vector<{cppType}>";
            }
            if (!field.IsRequired)
            {
                cppType = $"std::optional<{cppType}>";
            }
            text.Append("\t\t").Append(cppType).Append(' ').Append(column.MemberName).Append("{};\n");

            // 값이 큰 타입과 선택값은 행의 원래 멤버를 복사 없이 읽는다.
            bool returnsReference = field.ElementType != null || !field.IsRequired || scalarType == "string";
            getters.Append("\n\t\t[[nodiscard]]\n\t\t").Append(returnsReference ? $"const {cppType}&" : cppType)
                .Append(' ').Append(column.GetterName).Append("() const noexcept\n")
                .Append("\t\t{\n\t\t\treturn ").Append(column.MemberName).Append(";\n\t\t}\n");
        }
        text.Append(getters).Append("\t};\n\n");

        text.Append("\tnamespace ").Append(table.Name).Append("Format\n\t{\n");
        text.Append("\t\tinline constexpr std::uint16_t kFormatVersion = ").Append(FormatVersion).Append(";\n");
        text.Append("\t\tinline constexpr std::uint8_t kTarget = ").Append(targetId).Append(";\n");
        text.Append("\t\tinline constexpr char kTableName[] = \"").Append(table.Name).Append("\";\n");
        text.Append("\t\tinline constexpr std::uint8_t kSchemaHash[32] =\n\t\t{\n");
        for (int offset = 0; offset < schemaHash.Length; offset += 16)
        {
            text.Append("\t\t\t").AppendJoin(", ", schemaHash[offset..(offset + 16)].Select(value => $"0x{value:x2}"))
                .Append(",\n");
        }
        text.Append("\t\t};\n\t}\n}\n");
        return CsvReader.Utf8.GetBytes(text.ToString());
    }

    private static byte[] WriteEnums(EnumDefinitions definitions)
    {
        var text = new StringBuilder("#pragma once\n\nnamespace GameData\n{\n");
        string? currentType = null;
        foreach (string[] row in definitions.ToCsvTable().Rows.Skip(1))
        {
            if (currentType != row[0])
            {
                if (currentType != null)
                {
                    text.Append("\t};\n\n");
                }
                currentType = row[0];
                text.Append("\tenum class ").Append(currentType).Append(" : std::uint32_t\n\t{\n");
            }
            text.Append("\t\t").Append(row[1]).Append(" = ").Append(row[2]).Append("u,\n");
        }
        if (currentType != null)
        {
            text.Append("\t};\n");
        }
        text.Append("}\n");
        return CsvReader.Utf8.GetBytes(text.ToString());
    }

    private static byte[] WriteBinary(DataTable table, int[] projection, byte targetId, byte[] schemaHash)
    {
        using var stream = new MemoryStream();
        using var writer = new BinaryWriter(stream, CsvReader.Utf8);
        writer.Write("GDAT"u8);
        writer.Write(FormatVersion);
        writer.Write(targetId);
        WriteUtf8(writer, table.Name);
        writer.Write(schemaHash);
        writer.Write((uint)table.Rows.Count);
        foreach (DataRow row in table.Rows)
        {
            foreach (int index in projection)
            {
                FieldDeclaration field = table.Columns[index].Field;
                string? value = row.Values[index];
                if (!field.IsRequired)
                {
                    writer.Write(value != null ? (byte)1 : (byte)0);
                    if (value == null)
                    {
                        continue;
                    }
                }
                if (field.ElementType is string elementType)
                {
                    IReadOnlyList<string> items = ArrayValues.ReadItems(value!, elementType == "string")!;
                    writer.Write((uint)items.Count);
                    foreach (string item in items)
                    {
                        WriteScalar(writer, elementType, field.EnumMembers, item);
                    }
                }
                else
                {
                    WriteScalar(writer, field.Type, field.EnumMembers, value!);
                }
            }
        }
        return stream.ToArray();
    }

    private static void WriteScalar(BinaryWriter writer, string type, IReadOnlyDictionary<string, uint>? enumMembers, string value)
    {
        if (enumMembers != null)
        {
            writer.Write(enumMembers[value]);
            return;
        }
        switch (type)
        {
            case "int32":
                writer.Write(int.Parse(value, NumberStyles.AllowLeadingSign, CultureInfo.InvariantCulture));
                break;
            case "uint32":
                writer.Write(uint.Parse(value, NumberStyles.None, CultureInfo.InvariantCulture));
                break;
            case "float32":
                writer.Write(float.Parse(value, NumberStyles.Float, CultureInfo.InvariantCulture));
                break;
            case "string":
                WriteUtf8(writer, value);
                break;
            default:
                throw new InvalidDataException($"Cannot encode field type '{type}'.");
        }
    }

    private static void WriteUtf8(BinaryWriter writer, string value)
    {
        byte[] bytes = CsvReader.Utf8.GetBytes(value);
        writer.Write((uint)bytes.Length);
        writer.Write(bytes);
    }

    // 생성물과 최초 수기 파일을 함께 게시하되 기존 수기 파일은 건드리지 않는다.
    private static void Publish(string outputDirectory, Dictionary<string, byte[]> artifacts, Dictionary<string, byte[]> manualTables)
    {
        CheckStaleOutputs(outputDirectory, artifacts.Keys.ToHashSet(StringComparer.Ordinal));
        string? parent = Path.GetDirectoryName(Path.TrimEndingDirectorySeparator(outputDirectory));
        if (parent == null || string.Equals(Path.Combine(parent, "Tables"), Path.TrimEndingDirectorySeparator(outputDirectory),
            StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidDataException("The generated output must be separate from its sibling Tables directory.");
        }
        string manualDirectory = Path.Combine(parent, "Tables");
        using var batch = new OutputBatch(outputDirectory);
        foreach ((string path, byte[] bytes) in artifacts)
        {
            batch.Stage(Path.Combine(outputDirectory, path), bytes);
        }
        foreach ((string path, byte[] bytes) in manualTables)
        {
            string destination = Path.Combine(manualDirectory, path);
            if (!File.Exists(destination))
            {
                batch.Stage(destination, bytes, createOnly: true);
            }
        }
        batch.Commit();
        foreach (string path in artifacts.Keys)
        {
            Program.WriteDiagnostic(Console.Out, Path.Combine(outputDirectory, path));
        }
    }

    // 대상 변경으로 빠진 기존 파일을 새 산출물 묶음에 섞지 않는다.
    private static void CheckStaleOutputs(string outputDirectory, HashSet<string> expected)
    {
        if (!CheckArtifactRoot(outputDirectory))
        {
            return;
        }
        CheckArtifactRoot(Path.Combine(outputDirectory, "Data"));
        foreach (string relativeDirectory in new[] { "Common", "Server", "Client", Path.Combine("Data", "Server"), Path.Combine("Data", "Client") })
        {
            string directory = Path.Combine(outputDirectory, relativeDirectory);
            if (CheckArtifactRoot(directory))
            {
                CheckArtifactDirectory(new DirectoryInfo(directory), outputDirectory, expected);
            }
        }
    }

    private static void CheckArtifactDirectory(DirectoryInfo directory, string outputDirectory, HashSet<string> expected)
    {
        foreach (FileSystemInfo entry in directory.EnumerateFileSystemInfos())
        {
            // 링크를 따라가면 묶음 밖 파일이 섞이거나 검사를 건너뛰게 된다.
            if ((entry.Attributes & FileAttributes.ReparsePoint) != 0)
            {
                throw new InvalidDataException($"Linked artifact path '{Path.GetRelativePath(outputDirectory, entry.FullName)}'. Use a fresh output directory.");
            }
            if (entry is DirectoryInfo child)
            {
                CheckArtifactDirectory(child, outputDirectory, expected);
                continue;
            }
            string extension = entry.Extension.ToLowerInvariant();
            string path = Path.GetRelativePath(outputDirectory, entry.FullName);
            if (extension is ".h" or ".sdat" or ".cdat" or ".dat" && !expected.Contains(path))
            {
                throw new InvalidDataException($"Stale artifact '{path}'. Use a fresh output directory for the new selection.");
            }
        }
    }

    // 경로가 없을 때만 건너뛴다. 권한·I/O 오류는 호출자에게 그대로 전달한다.
    private static bool CheckArtifactRoot(string directory)
    {
        FileAttributes attributes;
        try
        {
            attributes = File.GetAttributes(directory);
        }
        catch (Exception exception) when (exception is FileNotFoundException or DirectoryNotFoundException)
        {
            return false;
        }
        if ((attributes & FileAttributes.ReparsePoint) != 0)
        {
            throw new InvalidDataException($"Linked artifact path '{directory}'. Use a fresh output directory.");
        }
        if ((attributes & FileAttributes.Directory) == 0)
        {
            throw new IOException($"Invalid artifact directory: '{directory}'.");
        }
        return true;
    }
}
