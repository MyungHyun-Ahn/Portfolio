using System.Globalization;
using ClosedXML.Excel;

internal sealed class EnumDefinitions
{
    internal const string WorkbookFileName = "Enum.xlsx";
    internal const string CsvName = "Enum";

    // 생성될 C++ 이름에 예약어와 프로젝트의 Windows 예약 토큰을 쓰지 않는다.
    private static readonly HashSet<string> ReservedNames = new("""
        alignas alignof and and_eq asm auto bitand bitor bool break case catch char char8_t char16_t char32_t
        class compl concept const const_cast consteval constexpr constinit continue co_await co_return co_yield
        decltype default delete do double dynamic_cast else enum explicit export extern false float for friend goto
        if inline int long mutable namespace new noexcept not not_eq nullptr operator or or_eq private protected
        public register reinterpret_cast requires return short signed sizeof static static_assert static_cast struct
        switch template this thread_local throw true try typedef typeid typename union unsigned using virtual void
        volatile wchar_t while xor xor_eq interface near far small pascal cdecl std
        """.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries), StringComparer.Ordinal);

    private readonly Dictionary<string, Dictionary<string, uint>> m_types = new(StringComparer.Ordinal);

    // 시트 단위로 정의를 읽은 뒤 일반 테이블에서 항목명을 조회한다.
    public void Read(XLWorkbook workbook, string fileName)
    {
        var typeNames = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (IXLWorksheet sheet in workbook.Worksheets)
        {
            string source = $"{fileName}:{sheet.Name}";
            if (!IsCppName(sheet.Name))
            {
                throw new InvalidDataException($"{source}!A1: Invalid enum type name.");
            }
            if (!typeNames.Add(sheet.Name))
            {
                throw new InvalidDataException($"{source}!A1: Duplicate enum type name '{sheet.Name}'.");
            }
            m_types.Add(sheet.Name, ReadMembers(sheet, source));
        }
        if (m_types.Count == 0)
        {
            throw new InvalidDataException($"{fileName}: At least one enum type sheet is required.");
        }
    }

    public IReadOnlyDictionary<string, uint>? Find(string typeName) => m_types.GetValueOrDefault(typeName);

    // 시트·행을 재배치해도 같은 이름과 번호로 같은 CSV를 만든다.
    public CsvTable ToCsvTable()
    {
        var rows = new List<string[]> { new[] { "Type", "Name", "Value" } };
        foreach ((string type, Dictionary<string, uint> members) in m_types.OrderBy(pair => pair.Key, StringComparer.Ordinal))
        {
            foreach ((string name, uint value) in members.OrderBy(pair => pair.Value))
            {
                rows.Add([type, name, value.ToString(CultureInfo.InvariantCulture)]);
            }
        }
        return new CsvTable(CsvName, rows);
    }

    private static Dictionary<string, uint> ReadMembers(IXLWorksheet sheet, string source)
    {
        InvalidDataException Error(IXLCell cell, string reason) => new($"{source}!{cell.Address}: {reason}");
        var dataRows = new SortedSet<int>();
        foreach (IXLCell cell in sheet.CellsUsed(XLCellsUsedOptions.AllContents))
        {
            if (cell.HasFormula)
            {
                throw Error(cell, "Formula cells are not allowed.");
            }
            if (cell.DataType == XLDataType.Error)
            {
                throw Error(cell, "Excel error cells are not allowed.");
            }
            if (cell.DataType == XLDataType.Blank || (cell.DataType == XLDataType.Text && cell.GetString().Length == 0))
            {
                continue;
            }
            if (cell.Address.ColumnNumber > 2)
            {
                throw Error(cell, "Use only columns A and B for enum definitions.");
            }
            if (cell.Address.RowNumber > 1)
            {
                dataRows.Add(cell.Address.RowNumber);
            }
        }

        IXLCell nameHeader = sheet.Cell(1, 1);
        IXLCell valueHeader = sheet.Cell(1, 2);
        if (nameHeader.DataType != XLDataType.Text || nameHeader.GetString() != "Name")
        {
            throw Error(nameHeader, "Expected 'Name' header.");
        }
        if (valueHeader.DataType != XLDataType.Text || valueHeader.GetString() != "Value")
        {
            throw Error(valueHeader, "Expected 'Value' header.");
        }
        if (dataRows.Count == 0)
        {
            throw Error(sheet.Cell(2, 1), "Declare at least one enum member.");
        }

        var members = new Dictionary<string, uint>(StringComparer.Ordinal);
        var numbers = new HashSet<uint>();
        foreach (int row in dataRows)
        {
            IXLCell nameCell = sheet.Cell(row, 1);
            IXLCell valueCell = sheet.Cell(row, 2);
            string name = nameCell.DataType == XLDataType.Text ? nameCell.GetString() : string.Empty;
            if (!IsCppName(name) || name == sheet.Name)
            {
                throw Error(nameCell, "Invalid enum member name.");
            }
            if (valueCell.DataType != XLDataType.Number ||
                FieldDeclarations.ReadNumber("uint32", valueCell.GetDouble()) is not FieldValue number)
            {
                throw Error(valueCell, "Enum value must be a uint32 integer in an Excel numeric cell.");
            }
            uint value = (uint)number.Number;
            if (!members.TryAdd(name, value))
            {
                throw Error(nameCell, $"Duplicate enum member '{name}'.");
            }
            if (!numbers.Add(value))
            {
                throw Error(valueCell, $"Duplicate enum value {number.Text}.");
            }
        }
        return members;
    }

    private static bool IsCppName(string name) => name.Length != 0 && char.IsAsciiLetter(name[0]) &&
        name.All(c => char.IsAsciiLetterOrDigit(c) || c == '_') && !name.Contains("__", StringComparison.Ordinal) &&
        !ReservedNames.Contains(name);
}
