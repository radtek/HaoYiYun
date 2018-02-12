// pages/live/live.js
// 定义直播类型...
// 0 => 共享通道...
// 1 => 个人通道...
const LIVE_SHARE = 0
const LIVE_PERSON = 1
Page({
  // 页面的初始数据
  data: {
    m_live_data: null,
    m_live_type: LIVE_SHARE
  },
  // 生命周期函数--监听页面加载
  onLoad: function (options) {
    // 这里获取的 m_live_data 是对象，不是数组...
    this.data.m_live_type = parseInt(options.type)
    this.data.m_live_data = JSON.parse(options.data)
    // 获取通道来源信息...
    switch( this.data.m_live_type ) {
      case LIVE_SHARE:  this.data.m_live_data.from_name = '共享通道'; break;
      case LIVE_PERSON: this.data.m_live_data.from_name = '我的通道'; break;
    }
    // 获取通道类型，根据通道属性...
    switch( this.data.m_live_data.stream_prop ) {
      case '0': this.data.m_live_data.stream_type = '摄像头设备'; break;
      case '1': this.data.m_live_data.stream_type = 'MP4文件'; break;
      case '2': this.data.m_live_data.stream_type = '协议转发'; break;
    }
    // 用通道名称设置标题名称，避免使用单一的“直播”字样...
    wx.setNavigationBarTitle({ title: this.data.m_live_data.camera_name })
    // 将通道详情设置到界面当中，展示出来...
    this.setData({ m_live_data: this.data.m_live_data })
  },
  // 页面相关事件处理函数--监听用户下拉动作
  onPullDownRefresh: function () {
    wx.stopPullDownRefresh()
  },
  // 页面上拉触底事件的处理函数
  onReachBottom: function () {
  },
  // 用户点击右上角分享
  onShareAppMessage: function () {
  }
})