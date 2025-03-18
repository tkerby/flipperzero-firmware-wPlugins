/**
 * NFC全息动画更新脚本
 * 
 * 这个脚本用于动态更新NFC全息动画中的内容
 * 可以根据实际需求修改数据标签、颜色和动画效果
 */

/**
 * 更新浮动标签文本
 * @param {Object} labels - 标签对象，键为标签ID，值为新文本
 */
function updateLabels(labels) {
  for (const [id, text] of Object.entries(labels)) {
    const element = document.querySelector(`.label-${id}`);
    if (element) {
      element.textContent = text;
    }
  }
}

/**
 * 更新十六进制数据集
 * @param {Array} newHexData - 新的十六进制数据集数组
 */
function updateHexData(newHexData) {
  if (Array.isArray(newHexData) && newHexData.length > 0) {
    window.hexDataSets = newHexData;
  }
}

/**
 * 更新技术参数
 * @param {Object} specs - 技术参数对象，键为参数名，值为新参数值
 */
function updateTechSpecs(specs) {
  for (const [label, value] of Object.entries(specs)) {
    const valueElement = document.querySelector(`.spec-item:has(.spec-label:contains('${label}')) .spec-value`);
    if (valueElement) {
      valueElement.textContent = value;
    }
  }
}

/**
 * 更新攻击步骤
 * @param {Array} steps - 新的攻击步骤数组
 */
function updateAttackSteps(steps) {
  if (Array.isArray(steps) && steps.length > 0) {
    const stepsContainer = document.querySelector('.attack-steps');
    if (stepsContainer) {
      stepsContainer.innerHTML = '';
      steps.forEach((step, index) => {
        const stepElement = document.createElement('div');
        stepElement.className = `step step-${index + 1}`;
        stepElement.textContent = step;
        stepsContainer.appendChild(stepElement);
      });
    }
  }
}

/**
 * 更新攻击标题
 * @param {string} title - 新的攻击标题
 */
function updateAttackTitle(title) {
  const titleElement = document.querySelector('.attack-title');
  if (titleElement) {
    titleElement.textContent = title;
  }
}

/**
 * 设置NFC标志颜色
 * @param {string} color - CSS颜色值
 */
function setNfcSymbolColor(color) {
  const nfcSymbol = document.querySelector('.nfc-symbol');
  if (nfcSymbol) {
    nfcSymbol.style.color = color;
  }
}

/**
 * 设置环形颜色
 * @param {Object} colors - 环形颜色对象，键为环形ID（1-3），值为颜色
 */
function setRingColors(colors) {
  for (const [id, color] of Object.entries(colors)) {
    const ring = document.querySelector(`.ring-${id}`);
    if (ring) {
      ring.style.borderColor = color;
    }
  }
}

// 导出函数以便在其他脚本中使用
if (typeof module !== 'undefined' && module.exports) {
  module.exports = {
    updateLabels,
    updateHexData,
    updateTechSpecs,
    updateAttackSteps,
    updateAttackTitle,
    setNfcSymbolColor,
    setRingColors
  };
} 