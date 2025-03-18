# NFC全息动画组件使用示例

以下是在Markdown文件中引用NFC全息动画组件的几种方式。

## 在README.md中使用iframe嵌入

如果你希望在GitHub的README.md文件中嵌入动画，可以使用以下代码：

```html
<div align="center">
  <iframe src="https://htmlpreview.github.io/?https://github.com/SpenserCai/nfc_apdu_runner/blob/main/.github/nfc_com/holographic-nfc-animation.html" 
          width="100%" 
          height="300px" 
          frameborder="0" 
          scrolling="no">
  </iframe>
</div>
```

请注意，GitHub的README.md默认不支持iframe嵌入，所以这种方法适用于其他支持HTML的文档平台。

## 在GitHub Pages中使用

如果你的仓库已启用GitHub Pages，可以直接在网页中嵌入：

```html
<div class="nfc-animation-container">
  <iframe src="https://spensercai.github.io/nfc_apdu_runner/.github/nfc_com/holographic-nfc-animation.html" 
          width="100%" 
          height="300px" 
          frameborder="0" 
          scrolling="no">
  </iframe>
</div>
```

## 在本地HTML文件中使用

1. 下载 `holographic-nfc-animation.html` 文件
2. 在你的HTML文件中添加：

```html
<div class="nfc-animation-container">
  <iframe src="path/to/holographic-nfc-animation.html" 
          width="100%" 
          height="300px" 
          frameborder="0" 
          scrolling="no">
  </iframe>
</div>
```

## 直接嵌入HTML代码

如果你希望直接在网页中使用（不使用iframe），可以复制 `holographic-nfc-animation.html` 文件中的HTML、CSS和JavaScript代码到你的网页中。

例如：

```html
<!DOCTYPE html>
<html>
<head>
  <title>我的NFC项目</title>
  <style>
    /* 这里粘贴holographic-nfc-animation.html中的CSS样式 */
  </style>
</head>
<body>
  <h1>我的NFC项目</h1>
  
  <!-- 这里粘贴holographic-nfc-animation.html中的HTML结构 -->
  
  <script>
    // 这里粘贴holographic-nfc-animation.html中的JavaScript代码
  </script>
</body>
</html>
```

## 自定义动画

如果你想自定义动画，可以使用 `update_animation.js` 中提供的函数：

```javascript
// 引入update_animation.js
// const animator = require('./update_animation.js');

// 更新浮动标签
updateLabels({
  1: "TLV: 9F2608",
  2: "UID: 01A2B3C4",
  // ...其他标签
});

// 更新十六进制数据
updateHexData([
  [
    '9F 26 08 3E DC 16 79 53 C6 14 22',
    '9F 27 01 80 9F 10 07 06 01 0A 03 60 00',
    '9F 36 02 06 8A 9F 6E 04 20 70 80 00'
  ],
  // ...其他数据集
]);

// 更新技术参数
updateTechSpecs({
  "ISO/IEC": "14443B",
  "Freq": "13.56MHz",
  "Crypto": "AES"
});

// 更新攻击步骤
updateAttackSteps([
  "读取UID",
  "发送APDU命令",
  "获取响应",
  "解析数据"
]);

// 更新攻击标题
updateAttackTitle("EMV卡片分析");

// 设置NFC标志颜色
setNfcSymbolColor("rgba(126, 231, 135, 0.9)");

// 设置环形颜色
setRingColors({
  1: "rgba(126, 231, 135, 0.8)",
  2: "rgba(56, 189, 248, 0.6)",
  3: "rgba(137, 87, 229, 0.4)"
});
```

## 注意事项

1. 动画使用了现代CSS特性，确保在支持这些特性的浏览器中查看
2. 如果动画显示不正常，请检查CSS样式是否有冲突
3. 为了获得最佳视觉效果，建议在深色背景上使用 