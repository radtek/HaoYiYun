// pages/live/live.js
// 获取全局的app对象...
const g_app = getApp()
// 本页面的代码...
Page({
  // 页面的初始数据...
  data: {
  },
  // 生命周期函数--监听页面加载...
  onLoad: function (options) {
    // 用通道名称设置标题栏...
    wx.setNavigationBarTitle({
      title: 'MP4文件 - 监控通道'
    })
  },
  // 下拉刷新事件...
  onPullDownRefresh: function () {
    wx.stopPullDownRefresh()
  },
  // 页面上拉触底事件的处理函数...
  onReachBottom: function () {
  },
  // 用户点击右上角分享...
  onShareAppMessage: function () {
  },
  // 用户点击切换按钮的事件...
  onSwitchLive: function() {
    
  }
})