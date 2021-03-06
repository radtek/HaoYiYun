// pages/share/share.js
// 获取全局的app对象...
const g_app = getApp()
// 本页面的代码...
Page({
  data: {
  },
  onLoad: function (options) {
    // 如果没有获取到用户编号或用户信息，跳转到默认的授权页面，重新获取授权...
    if( g_app.globalData.m_nUserID <= 0 || g_app.globalData.m_userInfo == null ) {
      wx.navigateTo({ url: '../default/default' })
      return
    }
    // 获取到的用户信息有效，弹出等待框...
    wx.showLoading({
      title: '加载中'
    })
    // 调用接口，获取共享通道列表数据 => 默认获取第一页...
    this.doAPIGetShare()
  },
  // 调用网站API获取共享通道列表...
  doAPIGetShare(inCurPage = 1) {
    // 构造访问接口连接地址...
    var theUrl = g_app.globalData.m_urlPrev + 'Mini/share/p/' + inCurPage
    // 请求远程API过程...
    wx.request({
      url: theUrl,
      method: 'GET',
      success: function (res) {
        var arrData = JSON.parse(res.data);
      },
      fail: function (res) {
      }
    })
  },
  // 下拉刷新事件...
  onPullDownRefresh: function() {
    console.log('onPullDownRefresh')
    wx.stopPullDownRefresh()
  },
  // 上拉触底事件...
  onReachBottom: function() {
    console.log('bottom')
  },
  // 处理截图加载失败的事件...
  doErrSnap: function() {
    console.log(event);
  },
  /*// 响应用户点击发布者事件...
  catchtap可以阻止后续的冒泡...
  doTapOwner: function () {
    console.log('doOwner')
  },*/
  // 响应用户点击单条记录事件...
  doTapItem: function() {
    wx.navigateTo({
      url: '../live/live'
    })
  }
})