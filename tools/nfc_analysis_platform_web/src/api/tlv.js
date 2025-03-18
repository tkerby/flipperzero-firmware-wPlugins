import api from './config';

// 解析 TLV 数据
export const parseTlvData = (data) => {
  return api.post('/tlv/parse', {
    hex_data: data
  });
};

// 从 TLV 数据中提取指定标签的值
export const extractTlvValues = (data, tags, dataType = 'hex') => {
  return api.post('/tlv/extract', {
    hex_data: data,
    tags: tags,
    data_type: dataType
  });
}; 