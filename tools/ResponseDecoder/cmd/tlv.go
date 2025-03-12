package cmd

import (
	"fmt"
	"strings"

	"github.com/fatih/color"
	"github.com/spensercai/nfc_apdu_runner/tools/ResponseDecoder/pkg/tlv"
	"github.com/spf13/cobra"
)

var (
	hexData  string
	tagList  string
	dataType string
)

// tlvCmd 表示tlv命令
var tlvCmd = &cobra.Command{
	Use:   "tlv",
	Short: "解析TLV数据并提取指定标签的值",
	Long: `解析TLV数据并提取指定标签的值。

示例:
  # 解析所有标签
  response_decoder tlv --hex 6F198407A0000000031010A50E500A4D617374657243617264

  # 解析指定标签
  response_decoder tlv --hex 6F198407A0000000031010A50E500A4D617374657243617264 --tag 84,50

  # 指定数据类型
  response_decoder tlv --hex 6F198407A0000000031010A50E500A4D617374657243617264 --tag 50 --type ascii`,
	RunE: func(cmd *cobra.Command, args []string) error {
		if hexData == "" {
			return fmt.Errorf("必须提供十六进制数据 (--hex)")
		}

		// 解析TLV数据
		tlvList, err := tlv.ParseHex(hexData)
		if err != nil {
			return fmt.Errorf("解析TLV数据失败: %w", err)
		}

		// 如果没有指定标签，则显示所有标签
		if tagList == "" {
			return displayAllTags(tlvList)
		}

		// 解析标签列表
		tags := strings.Split(tagList, ",")
		for _, tag := range tags {
			tag = strings.TrimSpace(tag)
			if tag == "" {
				continue
			}

			// 查找标签
			tlvItem, err := tlvList.FindTagRecursive(tag)
			if err != nil {
				fmt.Printf("标签 %s 未找到: %v\n", tag, err)
				continue
			}

			// 显示标签值
			displayTagValue(tag, tlvItem)
		}

		return nil
	},
}

// displayAllTags 显示所有标签
func displayAllTags(tlvList tlv.TLVList) error {
	titleColor := color.New(color.FgCyan, color.Bold)

	fmt.Println(strings.Repeat("=", 50))
	titleColor.Println("TLV 数据解析结果")
	fmt.Println(strings.Repeat("=", 50))

	// 递归显示TLV结构
	displayTLVStructure(tlvList, 0)

	return nil
}

// displayTLVStructure 递归显示TLV结构
func displayTLVStructure(tlvList tlv.TLVList, level int) {
	indent := strings.Repeat("  ", level)
	tagColor := color.New(color.FgYellow, color.Bold)
	valueColor := color.New(color.FgGreen)
	constructedColor := color.New(color.FgMagenta, color.Bold)

	for _, item := range tlvList {
		// 显示标签
		tagColor.Printf("%s标签: %s", indent, item.Tag)

		// 显示长度
		fmt.Printf(", 长度: %d", item.Length)

		// 如果是构造型标签，递归显示其内容
		if item.IsConstructed() {
			constructedColor.Println(" [构造型]")

			// 解析嵌套TLV
			nestedList, err := tlv.Parse(item.Value)
			if err != nil {
				fmt.Printf("%s  解析嵌套TLV失败: %v\n", indent, err)
				continue
			}

			// 递归显示嵌套TLV
			displayTLVStructure(nestedList, level+1)
		} else {
			// 显示值
			fmt.Print(", 值: ")

			// 根据数据类型显示值
			switch dataType {
			case "utf8", "utf-8":
				data, err := tlv.GetTagValueAsString(hexData, item.Tag.String(), "utf-8")
				if err != nil {
					valueColor.Printf("%s\n", item.Value)
				} else {
					valueColor.Printf("%s\n", data)
				}
			case "ascii":
				data, err := tlv.GetTagValueAsString(hexData, item.Tag.String(), "ascii")
				if err != nil {
					valueColor.Printf("%s\n", item.Value)
				} else {
					valueColor.Printf("%s\n", data)
				}
			case "numeric":
				data, err := tlv.GetTagValueAsString(hexData, item.Tag.String(), "numeric")
				if err != nil {
					valueColor.Printf("%s\n", item.Value)
				} else {
					valueColor.Printf("%s\n", data)
				}
			default:
				valueColor.Printf("%s\n", item.Value)
			}
		}
	}
}

// displayTagValue 显示标签值
func displayTagValue(tag string, tlvItem *tlv.TLV) {
	tagColor := color.New(color.FgYellow, color.Bold)
	valueColor := color.New(color.FgGreen)

	// 显示标签
	tagColor.Printf("标签: %s", tag)

	// 显示长度
	fmt.Printf(", 长度: %d", tlvItem.Length)

	// 显示值
	fmt.Print(", 值: ")

	// 根据数据类型显示值
	switch dataType {
	case "utf8", "utf-8":
		data, err := tlv.GetTagValueAsString(hexData, tag, "utf-8")
		if err != nil {
			valueColor.Printf("%s\n", tlvItem.Value)
		} else {
			valueColor.Printf("%s\n", data)
		}
	case "ascii":
		data, err := tlv.GetTagValueAsString(hexData, tag, "ascii")
		if err != nil {
			valueColor.Printf("%s\n", tlvItem.Value)
		} else {
			valueColor.Printf("%s\n", data)
		}
	case "numeric":
		data, err := tlv.GetTagValueAsString(hexData, tag, "numeric")
		if err != nil {
			valueColor.Printf("%s\n", tlvItem.Value)
		} else {
			valueColor.Printf("%s\n", data)
		}
	default:
		valueColor.Printf("%s\n", tlvItem.Value)
	}
}

func init() {
	rootCmd.AddCommand(tlvCmd)

	// 添加命令行标志
	tlvCmd.Flags().StringVar(&hexData, "hex", "", "要解析的十六进制TLV数据")
	tlvCmd.Flags().StringVar(&tagList, "tag", "", "要提取的标签列表，用逗号分隔")
	tlvCmd.Flags().StringVar(&dataType, "type", "", "数据类型 (utf8, ascii, numeric)")
}
