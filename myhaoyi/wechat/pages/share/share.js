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
    /*wx.showLoading({
      title: '加载中'
    })
    setTimeout(function () {
      wx.hideLoading()
    }, 2000)*/
    // 调用接口，获取共享通道列表数据...
  },
  // 下拉刷新事件...
  onPullDownRefresh: function() {
    console.log('onPullDownRefresh')
    wx.stopPullDownRefresh()
  },
  // 上拉触底事件...
  onReachBottom: function() {
    console.log('bottom')
  }
})