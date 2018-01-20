// pages/about/about.js

Page({
  // 页面的初始数据
  data: {
  },
  // 生命周期函数--监听页面加载
  onLoad: function (options) {
  },
  // 页面相关事件处理函数--监听用户下拉动作
  /*onPullDownRefresh: function () {
    wx.stopPullDownRefresh()
  },*/
  // 用户点击右上角分享
  onShareAppMessage: function () {
  },
  // 点击复制QQ群操作...
  doCopyClip: function(inEvent) {
    var theGroupData = inEvent.currentTarget.dataset.group
    wx.setClipboardData({
      data: theGroupData,
      success: function (res) {
        wx.showToast({
          title: '复制成功',
          icon: 'success'
        })
      }
    })
  }
})