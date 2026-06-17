import type { UIDLDocument, UIDLNode, FormValues } from '../../types/uidl'

/** Minimal UIDL doc — single input field */
export const MINIMAL_UIDL: UIDLDocument = {
  render_type: 'dynamic_form',
  version: '1.0.0',
  title: 'Minimal Test',
  fields: [
    {
      widget: 'input',
      label: '名称',
      key: 'name',
      required: true,
      placeholder: '输入名称',
      default_value: '',
      order: 1,
    },
  ],
}

/** Full-featured UIDL exercising all 8 widget types */
export const FULL_UIDL: UIDLDocument = {
  render_type: 'dynamic_form',
  version: '1.0.0',
  title: '数据库连接配置',
  description: '配置数据库连接参数',
  fields: [
    {
      widget: 'input',
      label: '主机地址',
      key: 'host',
      required: true,
      placeholder: 'localhost',
      default_value: 'localhost',
      validation: { max_length: 255 },
      order: 1,
    },
    {
      widget: 'number',
      label: '端口号',
      key: 'port',
      required: true,
      default_value: 3306,
      validation: { min: 1, max: 65535 },
      order: 2,
    },
    {
      widget: 'input',
      label: '数据库名',
      key: 'database',
      required: true,
      placeholder: 'mydb',
      validation: { max_length: 128 },
      order: 3,
    },
    {
      widget: 'input',
      label: '用户名',
      key: 'username',
      required: true,
      placeholder: 'root',
      order: 4,
    },
    {
      widget: 'input',
      label: '密码',
      key: 'password',
      required: true,
      placeholder: '••••••••',
      order: 5,
    },
    {
      widget: 'group',
      label: '连接池设置',
      key: 'pool',
      order: 6,
      children: [
        {
          widget: 'number',
          label: '最小连接数',
          key: 'min_size',
          default_value: 2,
          validation: { min: 1, max: 100 },
          order: 1,
        },
        {
          widget: 'number',
          label: '最大连接数',
          key: 'max_size',
          default_value: 10,
          validation: { min: 1, max: 200 },
          order: 2,
        },
        {
          widget: 'number',
          label: '超时时间(秒)',
          key: 'timeout',
          default_value: 30,
          validation: { min: 1, max: 3600 },
          order: 3,
        },
      ],
    },
    {
      widget: 'select',
      label: '字符集',
      key: 'charset',
      default_value: 'utf8mb4',
      options: [
        { label: 'UTF-8', value: 'utf8' },
        { label: 'UTF-8 MB4', value: 'utf8mb4' },
        { label: 'Latin1', value: 'latin1' },
        { label: 'GBK', value: 'gbk' },
      ],
      order: 7,
    },
    {
      widget: 'checkbox',
      label: '启用 SSL',
      key: 'ssl_enabled',
      default_value: false,
      order: 8,
    },
    {
      widget: 'repeatable',
      label: '额外参数',
      key: 'extra_params',
      order: 9,
      children: [
        {
          widget: 'input',
          label: '参数名',
          key: 'name',
          required: true,
          order: 1,
        },
        {
          widget: 'input',
          label: '参数值',
          key: 'value',
          required: true,
          order: 2,
        },
      ],
    },
  ],
}

/** Nested group 3-deep */
export const NESTED_GROUP_UIDL: UIDLDocument = {
  render_type: 'dynamic_form',
  version: '1.0.0',
  title: 'Nested Config',
  fields: [
    {
      widget: 'group',
      label: 'Level 1',
      key: 'l1',
      order: 1,
      children: [
        {
          widget: 'input',
          label: 'L1 Field',
          key: 'l1_field',
          order: 1,
        },
        {
          widget: 'group',
          label: 'Level 2',
          key: 'l2',
          order: 2,
          children: [
            {
              widget: 'input',
              label: 'L2 Field',
              key: 'l2_field',
              order: 1,
            },
            {
              widget: 'group',
              label: 'Level 3',
              key: 'l3',
              order: 2,
              children: [
                {
                  widget: 'number',
                  label: 'L3 Field',
                  key: 'l3_field',
                  default_value: 42,
                  order: 1,
                },
              ],
            },
          ],
        },
      ],
    },
  ],
}

/** Initial form values matching {@link FULL_UIDL} */
export const FULL_FORM_VALUES: FormValues = {
  host: 'localhost',
  port: 3306,
  database: 'mydb',
  username: 'root',
  password: '',
  pool: { min_size: 2, max_size: 10, timeout: 30 },
  charset: 'utf8mb4',
  ssl_enabled: false,
  extra_params: [{ name: 'key1', value: 'val1' }],
}
