package common

import (
	"encoding/json"
	"os"
	"path/filepath"
)

// TestData 结构体用于存储测试数据
type TestData struct {
	TLV struct {
		SampleData         string `json:"sample_data"`
		SampleDataExtended string `json:"sample_data_extended"`
	} `json:"tlv"`
	NARD struct {
		SampleData     json.RawMessage `json:"sample_data"`
		SampleResponse string          `json:"sample_response"`
	} `json:"nard"`
	FormatTemplate string `json:"format_template"`
}

var testData TestData

// SampleTLVData 是用于测试的示例TLV数据
var SampleTLVData string

// SampleTLVDataExtended 是用于API测试的扩展TLV数据
var SampleTLVDataExtended string

// SampleNARDData 是用于测试的示例NARD数据
var SampleNARDData string

// SampleAPDUResponse 是用于测试的示例APDU响应数据
var SampleAPDUResponse string

// SampleFormatTemplate 是用于测试的示例格式模板
var SampleFormatTemplate string

// 初始化函数，加载测试数据
func init() {
	// 尝试从项目根目录、当前目录和上级目录查找测试数据文件
	possiblePaths := []string{
		"tests/testdata.json",
		"testdata.json",
		"../testdata.json",
		"../../testdata.json",
	}

	var err error
	var data []byte

	for _, path := range possiblePaths {
		data, err = os.ReadFile(path)
		if err == nil {
			break
		}
	}

	if err != nil {
		// 如果找不到文件，使用默认值
		SampleTLVData = "6F198407A0000000031010A50E500A4D617374657243617264"
		SampleTLVDataExtended = "6F1A840E315041592E5359532E4444463031A5088801025F2D02zhCN"
		SampleNARDData = `{"format":"EMV","data":{"AID":"A0000000041010","Label":"Visa Credit"}}`
		SampleAPDUResponse = "Flipper NFC APDU Runner\nDevice: ISO14443-4A (MIFARE DESFire)\nUID: 04A23B9C5D7E8F\nATQA: 0004\nSAK: 08\n\nResponse:\nIn: 00A404000E315041592E5359532E4444463031\nOut: 6F2A840E315041592E5359532E4444463031A518BF0C15611361124F07A0000000031010870101500A4D617374657243617264 9000\nIn: 00B2010C00\nOut: 7081B89081B15A0842628D13FFFFFFFF82025800950500000080005F24032312315F25030101015F280208405F2A0208405F300202015F3401009F01060000000000019F02060000000001009F03060000000000009F0607A00000000310109F0702FF009F080200029F090200029F0D05B8508000009F0E0500000000009F0F05B8708098009F100706010A03A020009F120A4D6173746572436172649F160F3132333435363738393031323334359F1A0208409F1C0831323334353637389F1E0831323334353637389F33036028C89F34030203009F3501229F360200019F3704AAAAAAAA 9000"
		SampleFormatTemplate = "EMV Card Information\nApplication Label: {O[1]TAG(50), \"ascii\"}\nCard Number: {O[3]TAG(9F6B)[0:16], \"numeric\"}\nExpiry Date: Year: 20{O[3]TAG(9F6B)[17:19], \"numeric\"}, Month: {O[3]TAG(9F6B)[19:21], \"numeric\"}\nCountry Code: {O[1]TAG(9F6E)[0:4]}\nApplication Priority: {O[1]TAG(87), \"hex\"}\nApplication Interchange Profile: {O[2]TAG(82), \"hex\"}\nPIN Try Counter: {O[4]TAG(9F17), \"numeric\"}"
		return
	}

	// 解析JSON数据
	err = json.Unmarshal(data, &testData)
	if err != nil {
		// 如果解析失败，使用默认值
		SampleTLVData = "6F198407A0000000031010A50E500A4D617374657243617264"
		SampleTLVDataExtended = "6F1A840E315041592E5359532E4444463031A5088801025F2D02zhCN"
		SampleNARDData = `{"format":"EMV","data":{"AID":"A0000000041010","Label":"Visa Credit"}}`
		SampleAPDUResponse = "Flipper NFC APDU Runner\nDevice: ISO14443-4A (MIFARE DESFire)\nUID: 04A23B9C5D7E8F\nATQA: 0004\nSAK: 08\n\nResponse:\nIn: 00A404000E315041592E5359532E4444463031\nOut: 6F2A840E315041592E5359532E4444463031A518BF0C15611361124F07A0000000031010870101500A4D617374657243617264 9000\nIn: 00B2010C00\nOut: 7081B89081B15A0842628D13FFFFFFFF82025800950500000080005F24032312315F25030101015F280208405F2A0208405F300202015F3401009F01060000000000019F02060000000001009F03060000000000009F0607A00000000310109F0702FF009F080200029F090200029F0D05B8508000009F0E0500000000009F0F05B8708098009F100706010A03A020009F120A4D6173746572436172649F160F3132333435363738393031323334359F1A0208409F1C0831323334353637389F1E0831323334353637389F33036028C89F34030203009F3501229F360200019F3704AAAAAAAA 9000"
		SampleFormatTemplate = "EMV Card Information\nApplication Label: {O[1]TAG(50), \"ascii\"}\nCard Number: {O[3]TAG(9F6B)[0:16], \"numeric\"}\nExpiry Date: Year: 20{O[3]TAG(9F6B)[17:19], \"numeric\"}, Month: {O[3]TAG(9F6B)[19:21], \"numeric\"}\nCountry Code: {O[1]TAG(9F6E)[0:4]}\nApplication Priority: {O[1]TAG(87), \"hex\"}\nApplication Interchange Profile: {O[2]TAG(82), \"hex\"}\nPIN Try Counter: {O[4]TAG(9F17), \"numeric\"}"
		return
	}

	// 设置全局变量
	SampleTLVData = testData.TLV.SampleData
	SampleTLVDataExtended = testData.TLV.SampleDataExtended
	SampleNARDData = string(testData.NARD.SampleData)
	SampleAPDUResponse = testData.NARD.SampleResponse
	SampleFormatTemplate = testData.FormatTemplate
}

// CreateTestDataFiles 创建测试数据文件
func CreateTestDataFiles(testDataDir string) error {
	// 创建 APDU 响应文件
	apduResFile := filepath.Join(testDataDir, "sample.apdures")
	if err := os.WriteFile(apduResFile, []byte(SampleAPDUResponse), 0644); err != nil {
		return err
	}

	// 创建格式模板文件
	formatFile := filepath.Join(testDataDir, "sample.apdufmt")
	if err := os.WriteFile(formatFile, []byte(SampleFormatTemplate), 0644); err != nil {
		return err
	}

	return nil
}
