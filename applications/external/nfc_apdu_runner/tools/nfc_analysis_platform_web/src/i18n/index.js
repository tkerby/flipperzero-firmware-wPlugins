import { createI18n } from 'vue-i18n'
import Cookies from 'js-cookie'
import en from './en'
import zh from './zh'

// 获取浏览器语言
const getBrowserLanguage = () => {
  const lang = navigator.language || navigator.userLanguage
  return lang.toLowerCase().startsWith('zh') ? 'zh' : 'en'
}

// 从 cookie 获取语言设置或使用默认值
const getLanguage = () => {
  const cookieLang = Cookies.get('nap_lang')
  if (cookieLang && ['en', 'zh'].includes(cookieLang)) {
    return cookieLang
  }
  // 如果 cookie 中没有语言设置或语言设置无效，使用浏览器语言
  const browserLang = getBrowserLanguage()
  Cookies.set('nap_lang', browserLang, { expires: 365 })
  return browserLang
}

// 创建 i18n 实例
const i18n = createI18n({
  legacy: false, // 使用 Composition API
  locale: getLanguage(), // 使用 getLanguage 获取初始语言
  fallbackLocale: 'en', // 设置回退语言为英文
  messages: {
    en,
    zh
  }
})

// 导出语言切换函数
export const setLanguage = (lang) => {
  if (['en', 'zh'].includes(lang)) {
    i18n.global.locale.value = lang
    Cookies.set('nap_lang', lang, { expires: 365 })
  }
}

export default i18n 