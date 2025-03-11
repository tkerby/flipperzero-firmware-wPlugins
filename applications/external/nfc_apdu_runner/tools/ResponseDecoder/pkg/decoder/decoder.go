package decoder

import (
	"bufio"
	"encoding/hex"
	"fmt"
	"io/ioutil"
	"os"
	"regexp"
	"strconv"
	"strings"

	"github.com/fatih/color"
)

// Response 表示解析后的APDU响应
type Response struct {
	Inputs  []string
	Outputs []string
}

// ParseResponseData 解析.apdures文件内容
func ParseResponseData(data string) (*Response, error) {
	scanner := bufio.NewScanner(strings.NewReader(data))
	response := &Response{
		Inputs:  []string{},
		Outputs: []string{},
	}

	// 跳过文件头
	for scanner.Scan() {
		line := scanner.Text()
		if line == "Response:" {
			break
		}
	}

	// 解析输入和输出
	for scanner.Scan() {
		line := scanner.Text()
		if strings.HasPrefix(line, "In: ") {
			response.Inputs = append(response.Inputs, strings.TrimPrefix(line, "In: "))
		} else if strings.HasPrefix(line, "Out: ") {
			response.Outputs = append(response.Outputs, strings.TrimPrefix(line, "Out: "))
		}
	}

	if err := scanner.Err(); err != nil {
		return nil, fmt.Errorf("error scanning response data: %w", err)
	}

	return response, nil
}

// ListFormatFiles 列出格式目录下的所有.apdufmt文件
func ListFormatFiles(formatDir string) ([]string, error) {
	files, err := ioutil.ReadDir(formatDir)
	if err != nil {
		return nil, fmt.Errorf("error reading format directory: %w", err)
	}

	var formatFiles []string
	for _, file := range files {
		if !file.IsDir() && strings.HasSuffix(file.Name(), ".apdufmt") {
			formatFiles = append(formatFiles, file.Name())
		}
	}

	return formatFiles, nil
}

// SelectFormat 让用户选择格式文件
func SelectFormat(formats []string) (string, error) {
	if len(formats) == 0 {
		return "", fmt.Errorf("no format files found")
	}

	fmt.Println("Available format files:")
	for i, format := range formats {
		fmt.Printf("%d. %s\n", i+1, format)
	}

	fmt.Print("Select a format (1-", len(formats), "): ")
	var choice int
	_, err := fmt.Scanf("%d", &choice)
	if err != nil {
		return "", fmt.Errorf("error reading user input: %w", err)
	}

	if choice < 1 || choice > len(formats) {
		return "", fmt.Errorf("invalid choice: %d", choice)
	}

	return formats[choice-1], nil
}

// DecodeAndDisplay 解码并显示结果
func DecodeAndDisplay(response *Response, formatData string) error {
	lines := strings.Split(formatData, "\n")
	if len(lines) == 0 {
		return fmt.Errorf("empty format data")
	}

	// 显示标题
	title := lines[0]
	titleColor := color.New(color.FgCyan, color.Bold)
	fmt.Println(strings.Repeat("=", 50))
	titleColor.Printf("%s\n", centerText(title, 50))
	fmt.Println(strings.Repeat("=", 50))

	// 解析和显示其余行
	for i := 1; i < len(lines); i++ {
		line := lines[i]
		if line == "" {
			fmt.Println()
			continue
		}

		// 解析占位符
		result, err := parsePlaceholders(line, response)
		if err != nil {
			return fmt.Errorf("error parsing line %d: %w", i+1, err)
		}

		fmt.Println(result)
	}

	return nil
}

// centerText 将文本居中
func centerText(text string, width int) string {
	if len(text) >= width {
		return text
	}

	padding := (width - len(text)) / 2
	return strings.Repeat(" ", padding) + text
}

// parsePlaceholders 解析并替换占位符
func parsePlaceholders(line string, response *Response) (string, error) {
	re := regexp.MustCompile(`\{([^}]+)\}`)
	result := re.ReplaceAllStringFunc(line, func(match string) string {
		// 去除大括号
		expr := match[1 : len(match)-1]

		// 解析表达式
		value, err := evaluateExpression(expr, response)
		if err != nil {
			fmt.Fprintf(os.Stderr, "Warning: Error evaluating expression '%s': %s\n", expr, err)
			return match
		}

		return value
	})

	return result, nil
}

// evaluateExpression 评估表达式
func evaluateExpression(expr string, response *Response) (string, error) {
	// 检查是否是hex函数
	if strings.HasPrefix(expr, "hex(") && strings.HasSuffix(expr, ")") {
		// 提取hex函数的参数
		args := expr[4 : len(expr)-1]
		parts := strings.Split(args, ",")

		var dataExpr string
		var encoding string

		if len(parts) == 1 {
			dataExpr = strings.TrimSpace(parts[0])
			encoding = "gbk" // 默认编码
		} else if len(parts) == 2 {
			dataExpr = strings.TrimSpace(parts[0])
			encoding = strings.Trim(strings.TrimSpace(parts[1]), "\"'")
		} else {
			return "", fmt.Errorf("invalid number of arguments for hex function")
		}

		// 获取数据
		data, err := evaluateDataExpression(dataExpr, response)
		if err != nil {
			return "", err
		}

		// 解码十六进制
		decoded, err := hexDecode(data, encoding)
		if err != nil {
			return "", err
		}

		return decoded, nil
	}

	// 直接评估数据表达式
	return evaluateDataExpression(expr, response)
}

// evaluateDataExpression 评估数据表达式 (如 O[1][10:18])
func evaluateDataExpression(expr string, response *Response) (string, error) {
	// 匹配 O[index][slice] 或 I[index][slice] 格式
	re := regexp.MustCompile(`([OI])\[(\d+)\](?:\[([^]]+)\])?`)
	matches := re.FindStringSubmatch(expr)

	if matches == nil {
		return "", fmt.Errorf("invalid data expression format: %s", expr)
	}

	// 确定是输入还是输出
	var data []string
	if matches[1] == "O" {
		data = response.Outputs
	} else {
		data = response.Inputs
	}

	// 获取索引
	index, err := strconv.Atoi(matches[2])
	if err != nil {
		return "", fmt.Errorf("invalid index: %s", matches[2])
	}

	if index < 0 || index >= len(data) {
		return "", fmt.Errorf("index out of range: %d", index)
	}

	// 获取数据
	value := data[index]

	// 如果有切片表达式，应用它
	if len(matches) > 3 && matches[3] != "" {
		value, err = applySlice(value, matches[3])
		if err != nil {
			return "", err
		}
	}

	return value, nil
}

// applySlice 应用切片表达式 (如 10:18)
func applySlice(value string, sliceExpr string) (string, error) {
	parts := strings.Split(sliceExpr, ":")

	if len(parts) != 2 {
		return "", fmt.Errorf("invalid slice expression: %s", sliceExpr)
	}

	var start, end int
	var err error

	// 解析起始位置
	if parts[0] == "" {
		start = 0
	} else {
		start, err = strconv.Atoi(parts[0])
		if err != nil {
			return "", fmt.Errorf("invalid start index: %s", parts[0])
		}
	}

	// 解析结束位置
	if parts[1] == "" {
		end = len(value)
	} else {
		end, err = strconv.Atoi(parts[1])
		if err != nil {
			return "", fmt.Errorf("invalid end index: %s", parts[1])
		}
	}

	// 应用切片
	if start < 0 {
		start = len(value) + start
	}
	if end < 0 {
		end = len(value) + end
	}

	if start < 0 || start > len(value) || end < 0 || end > len(value) || start > end {
		return "", fmt.Errorf("slice indices out of range: %s", sliceExpr)
	}

	return value[start:end], nil
}

// hexDecode 解码十六进制字符串
func hexDecode(hexStr string, encoding string) (string, error) {
	// 解码十六进制
	data, err := hex.DecodeString(hexStr)
	if err != nil {
		return "", fmt.Errorf("error decoding hex: %w", err)
	}

	// 根据编码转换为字符串
	var result string

	switch encoding {
	case "utf-8", "utf8":
		result = string(data)
	case "gbk":
		// 简单处理：保留可见字符
		var sb strings.Builder
		for _, b := range data {
			if b >= 32 && b <= 126 {
				sb.WriteByte(b)
			}
		}
		result = sb.String()
	default:
		// 默认处理：保留可见字符
		var sb strings.Builder
		for _, b := range data {
			if b >= 32 && b <= 126 {
				sb.WriteByte(b)
			}
		}
		result = sb.String()
	}

	// 清理结果，只保留有效内容
	result = cleanString(result)

	return result, nil
}

// cleanString 清理字符串，只保留有效内容
func cleanString(s string) string {
	// 保留中文、英文、数字、空格等
	re := regexp.MustCompile(`[^\p{Han}\p{Latin}\p{N}\s.,;:!?()[\]{}'"+-*/=<>@#$%^&_|\\]+`)
	return re.ReplaceAllString(s, "")
}
