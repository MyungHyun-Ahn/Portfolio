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
        foreach (string name in selected.Order(StringComparer.Ordinal))
        {
            tables.Add(name, CsvDataTables.Read(csvDirectory, name, enumDefinitions));
        }
        CsvDataTables.ValidateReferences(tables);

        var artifacts = new Dictionary<string, byte[]>(StringComparer.OrdinalIgnoreCase)
        {
            [Path.Combine("Common", "GameDataEnums.h")] = WriteEnums(enumDefinitions),
        };
        foreach (DataTable table in tables.Values)
        {
            AddProjection(table, "Server", 1, ".sdat", artifacts);
            AddProjection(table, "Client", 2, ".dat", artifacts);
        }
        Publish(outputDirectory, artifacts);
    }

    // 같은 필드 목록과 스키마 식별자를 헤더와 바이너리에서 사용한다.
    private static void AddProjection(DataTable table, string target, byte targetId, string extension, Dictionary<string, byte[]> artifacts)
    {
        if (!table.IncludesTarget(target))
        {
            return;
        }
        int[] projection = Enumerable.Range(0, table.Columns.Length)
            .Where(index => CsvDataTables.IsProjected(table, table.Columns[index], target)).ToArray();
        byte[] schemaHash = WriteSchemaHash(table, projection);
        artifacts.Add(Path.Combine(target, table.Name + ".h"), WriteHeader(table, projection, target, targetId, schemaHash));
        artifacts.Add(Path.Combine(target, table.Name + extension), WriteBinary(table, projection, targetId, schemaHash));
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
        text.Append("namespace GameData::").Append(target).Append("\n{\n");
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

    // 모든 후보를 먼저 쓴다. 전체 파일 교체의 복구는 Phase 0.7의 범위다.
    private static void Publish(string outputDirectory, Dictionary<string, byte[]> artifacts)
    {
        CheckStaleOutputs(outputDirectory, artifacts.Keys.ToHashSet(StringComparer.Ordinal));
        string candidate = Path.Combine(outputDirectory, $".game-data-output-{Guid.NewGuid():N}");
        foreach (string path in artifacts.Keys)
        {
            string destination = Path.Combine(outputDirectory, path);
            if (Directory.Exists(destination) || File.Exists(Path.GetDirectoryName(destination)))
            {
                throw new IOException($"Invalid artifact destination: {destination}.");
            }
        }
        Directory.CreateDirectory(candidate);
        try
        {
            foreach ((string path, byte[] bytes) in artifacts)
            {
                string staged = Path.Combine(candidate, path);
                Directory.CreateDirectory(Path.GetDirectoryName(staged)!);
                File.WriteAllBytes(staged, bytes);
            }
            foreach (string path in artifacts.Keys)
            {
                Directory.CreateDirectory(Path.GetDirectoryName(Path.Combine(outputDirectory, path))!);
            }
            foreach (string path in artifacts.Keys)
            {
                File.Move(Path.Combine(candidate, path), Path.Combine(outputDirectory, path), overwrite: true);
            }
        }
        finally
        {
            try
            {
                Directory.Delete(candidate, recursive: true);
            }
            catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
            {
                Console.Error.WriteLine($"Warning: Could not clean '{candidate}': {exception.Message}");
            }
        }
        foreach (string path in artifacts.Keys)
        {
            Console.WriteLine(Path.Combine(outputDirectory, path));
        }
    }

    // 대상 변경으로 빠진 기존 파일을 새 산출물 묶음에 섞지 않는다.
    private static void CheckStaleOutputs(string outputDirectory, HashSet<string> expected)
    {
        foreach (string target in new[] { "Common", "Server", "Client" })
        {
            string directory = Path.Combine(outputDirectory, target);
            if (!Directory.Exists(directory))
            {
                continue;
            }
            foreach (string file in Directory.EnumerateFiles(directory))
            {
                string extension = Path.GetExtension(file).ToLowerInvariant();
                string path = Path.Combine(target, Path.GetFileName(file));
                if (extension is ".h" or ".sdat" or ".dat" && !expected.Contains(path))
                {
                    throw new InvalidDataException($"Stale artifact '{path}'. Use a fresh output directory for the new selection.");
                }
            }
        }
    }
}
