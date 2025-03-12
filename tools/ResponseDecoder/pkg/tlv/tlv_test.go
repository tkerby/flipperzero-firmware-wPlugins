package tlv

import (
	"encoding/hex"
	"testing"
)

func TestParseTLV(t *testing.T) {
	// 测试数据
	testCases := []struct {
		name     string
		hexData  string
		expected int // 期望解析出的TLV数量
	}{
		{
			name:     "Simple TLV",
			hexData:  "8401FF",
			expected: 1,
		},
		{
			name:     "Multiple TLVs",
			hexData:  "8401FF9001AA5003112233",
			expected: 3,
		},
		{
			name:     "Nested TLVs",
			hexData:  "6F068401FF500101",
			expected: 1, // 顶层只有一个6F
		},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			tlvList, err := ParseHex(tc.hexData)
			if err != nil {
				t.Fatalf("Error parsing TLV: %v", err)
			}

			if len(tlvList) != tc.expected {
				t.Errorf("Expected %d TLVs, got %d", tc.expected, len(tlvList))
			}
		})
	}
}

func TestFindTag(t *testing.T) {
	// 测试数据
	hexData := "6F068401FF500101"

	tlvList, err := ParseHex(hexData)
	if err != nil {
		t.Fatalf("Error parsing TLV: %v", err)
	}

	// 测试查找标签
	tag, err := tlvList.FindTagRecursive("84")
	if err != nil {
		t.Fatalf("Error finding tag 84: %v", err)
	}

	expectedValue := "ff"
	if tag.Value.String() != expectedValue {
		t.Errorf("Expected value %s, got %s", expectedValue, tag.Value.String())
	}
}

func TestGetTagValue(t *testing.T) {
	// 测试数据
	hexData := "6F068401FF500101"

	// 测试获取标签值
	value, err := GetTagValue(hexData, "84")
	if err != nil {
		t.Fatalf("Error getting tag value: %v", err)
	}

	expectedValue := "ff"
	if value != expectedValue {
		t.Errorf("Expected value %s, got %s", expectedValue, value)
	}
}

func TestIsConstructed(t *testing.T) {
	// 测试构造型标签
	constructedTag := TLV{
		Tag: []byte{0x6F}, // 构造型标签
	}
	if !constructedTag.IsConstructed() {
		t.Errorf("Expected tag 6F to be constructed")
	}

	// 测试原始型标签
	primitiveTag := TLV{
		Tag: []byte{0x84}, // 原始型标签
	}
	if primitiveTag.IsConstructed() {
		t.Errorf("Expected tag 84 to be primitive")
	}
}

func TestParseComplexTLV(t *testing.T) {
	// 复杂的EMV数据 - 确保长度字段与实际数据长度匹配
	hexData := "6F178407A0000000031010A50C500A4D617374657243617264"

	tlvList, err := ParseHex(hexData)
	if err != nil {
		t.Fatalf("Error parsing TLV: %v", err)
	}

	// 验证解析结果
	if len(tlvList) != 1 {
		t.Errorf("Expected 1 top-level TLV, got %d", len(tlvList))
	}

	// 测试递归查找
	appLabel, err := tlvList.FindTagRecursive("50")
	if err != nil {
		t.Fatalf("Error finding Application Label (tag 50): %v", err)
	}

	// 解码应用标签
	appLabelBytes, err := hex.DecodeString(appLabel.Value.String())
	if err != nil {
		t.Fatalf("Error decoding application label: %v", err)
	}

	expectedLabel := "MasterCard"
	if string(appLabelBytes) != expectedLabel {
		t.Errorf("Expected application label %s, got %s", expectedLabel, string(appLabelBytes))
	}
}
