# NFC全息动画组件

这是一个用于在GitHub README中嵌入的NFC全息动画组件。该组件完全使用HTML、CSS和JavaScript实现，无需任何外部依赖。

## 使用方法

有两种方式可以在你的GitHub README文件中使用这个组件：

### 方法1：使用iframe嵌入（推荐）

在你的README.md文件中添加以下HTML代码：

```html
<div align="center">
  <iframe src="https://htmlpreview.github.io/?https://github.com/YourUsername/YourRepo/blob/main/.github/nfc_com/holographic-nfc-animation.html" 
          width="100%" 
          height="300px" 
          frameborder="0" 
          scrolling="no">
  </iframe>
</div>
```

请替换 `YourUsername` 和 `YourRepo` 为你的GitHub用户名和仓库名。

### 方法2：使用GitHub Pages

如果你的仓库已经启用了GitHub Pages，你可以直接链接到该HTML文件：

1. 确保你的仓库已启用GitHub Pages
2. 在README.md中添加以下代码：

```html
<div align="center">
  <iframe src="https://YourUsername.github.io/YourRepo/.github/nfc_com/holographic-nfc-animation.html" 
          width="100%" 
          height="300px" 
          frameborder="0" 
          scrolling="no">
  </iframe>
</div>
```

## 自定义

如果你想自定义动画，可以直接编辑 `holographic-nfc-animation.html` 文件：

- 修改标签文本内容
- 调整颜色和动画效果
- 添加或删除元素

## 注意事项

- GitHub的README默认不支持运行JavaScript，因此需要使用iframe或其他方法嵌入
- 某些环境可能会阻止iframe加载，所以建议提供备用的静态图片
- 该组件的设计适用于深色主题，在浅色主题下可能需要调整颜色 