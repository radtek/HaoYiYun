// pages/person/person.js
// 获取全局的app对象...
const g_app = getApp()
Page({
  // 页面的初始数据
  data: {
  },
  // 生命周期函数--监听页面加载
  onLoad: function (options) {
    // 如果没有获取到用户编号或用户信息，跳转到默认的授权页面，重新获取授权...
    if (g_app.globalData.m_nUserID <= 0 || g_app.globalData.m_userInfo == null) {
      wx.navigateTo({ url: '../default/default' })
      return
    }
    // 显示导航栏加载动画...
    wx.showNavigationBarLoading()
    // 调整头像的尺寸...
    var theUserInfo = g_app.globalData.m_userInfo
    var theHeadUrl = theUserInfo.avatarUrl.replace(/\/0/, '/132')
    var theNickName = theUserInfo.nickName
    this.setData({ m_headUrl: theHeadUrl, m_nickName: theNickName })
  },
  // 用户头像加载完毕通知 => 隐藏加载框...
  doHeadLoad: function (inEvent) {
    wx.hideNavigationBarLoading()
  },
  // 用户头像加载失败通知 => 隐藏加载框...
  doHeadErr: function (inEvent) {
    wx.hideNavigationBarLoading()
  },
  // 页面相关事件处理函数--监听用户下拉动作
  onPullDownRefresh: function () {
    wx.stopPullDownRefresh()
  },
  // 用户点击右上角分享...
  onShareAppMessage: function (res) {
    // 如果是自定义的按钮，特殊转发...
    if (res.from == 'button') {
      return {
        title: '云监控、云录播',
        path: 'pages/share/share',
        success: function (res) {
          console.log(res)
        },
        fail: function (res) {
          console.log(res)
        }
      }
    }
  }
})