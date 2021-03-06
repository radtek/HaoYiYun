// pages/default/default.js
// 定义授权状态...
// 0 => 等待验证授权...
// 1 => 获取授权成功...
// 2 => 获取授权失败...
const g_app = getApp()
Page({
  data: {
    m_code: '',
    m_nAuthState: 0,
    m_strErrInfo: '',
    m_strAuthInfo: '正在等待授权...',
  },
  onLoad: function (options) {
    // 保存this对象...
    var that = this
    // 处理登录过程...
    wx.login({
      success: res => {
        //保存code，后续使用...
        that.setData({m_code: res.code})
        // 立即读取用户信息，第一次会弹授权框...
        wx.getUserInfo({
          lang: 'zh_CN',
          withCredentials: true,
          success: res => {
            // 获取成功，通过网站接口获取用户编号...
            that.doAPILogin(that.data.m_code, res.userInfo, res.encryptedData, res.iv)
          },
          fail: res => {
            // 设置失败状态...
            that.setData({ m_nAuthState: 2, m_strErrInfo: '您点击了拒绝授权' })
          }
        })
      }
    })
  },
  // 调用网站API获取用户编号...
  doAPILogin: function(inCode, inUserInfo, inEncrypt, inIV) {
    // 保存this对象...
    var that = this
    // 获取授权成功，设置状态...
    that.setData({ m_nAuthState: 1, m_strAuthInfo: '正在获取授权数据...' })
    // 构造访问接口连接地址...
    var theUrl = g_app.globalData.m_urlPrev + 'Mini/login'
    // 请求远程API过程...
    wx.request({
      url: theUrl,
      method: 'POST',
      dataType: 'x-www-form-urlencoded',
      header: { 'content-type': 'application/x-www-form-urlencoded' },
      data: { code: inCode, encrypt: inEncrypt, iv: inIV },
      success: function (res) {
        var arrData = JSON.parse(res.data);
        // 获取授权数据失败的处理...
        if( arrData.errcode > 0 ) {
          that.setData({ m_nAuthState: 2, m_strErrInfo: arrData.errmsg })
          return;
        }
        // 获取授权数据成功，保存用户编号到全局对象...
        g_app.globalData.m_nUserID = arrData.user_id
        g_app.globalData.m_userInfo = inUserInfo
        // 获取授权成功，设置状态，并跳转到 共享通道 页面...
        that.setData({ m_nAuthState: 1, m_strAuthInfo: '授权成功，正在跳转...'})
        // 注意：navigateTo不能跳转tab页面，只能用switchTab...
        wx.switchTab({ url: '../share/share' })
      },
      fail: function (res) {
        // 发生错误，需要重新授权...
        that.setData({ m_nAuthState: 2, m_strErrInfo: '调用数据接口失败' })
      }
    })
  },
  // 点击获取授权按钮结果处理...
  getUserInfo: function(res) {
    // 保存this对象...
    var that = this
    // 授权成功，通过网站接口获取用户编号...
    if (res.detail.userInfo) {
      that.doAPILogin(that.data.m_code, res.detail.userInfo, res.detail.encryptedData, res.detail.iv)
    }
  }
})