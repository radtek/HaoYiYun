// pages/live/live.js
// 定义直播类型...
// 0 => 共享通道...
// 1 => 个人通道...
const LIVE_SHARE  = 0
const LIVE_PERSON = 1
// 获取全局的app对象...
const g_app = getApp()
// 本页面的代码...
Page({
  // 页面的初始数据...
  data: {
    m_cur_page: 1,
    m_max_page: 1,
    m_total_num: 0,
    m_arrRecord: [],
    m_is_error: false,
    m_live_data: null,
    m_live_type: LIVE_SHARE
  },
  // 生命周期函数--监听页面加载...
  onLoad: function (options) {
    // 用通道名称设置标题栏...
    //wx.setNavigationBarTitle({
    //  title: 'MP4文件 - 监控通道'
    //})
    // 这里获取的 m_live_data 是对象，不是数组...
    this.data.m_live_type = parseInt(options.type)
    this.data.m_live_data = JSON.parse(options.data)
    // 传递的信息有效，弹出等待框...
    wx.showLoading({ title: '加载中' })
    // 获取通道的直播播放地址...
    this.doAPIGetLiveAddr()
    // 初始化录像加载更多状态框...
    this.setData({m_show_more: true})
    // 获取当前通道下相关的录像列表...
    this.doAPIGetRecord()
  },
  // 调用网站API获取通道下的录像列表...
  doAPIGetRecord: function() {
    // 保存this对象...
    var that = this
    // 准备需要的参数信息...
    var thePostData = {
      'node_proto': that.data.m_live_data['node_proto'],
      'node_addr': that.data.m_live_data['node_addr'],
      'camera_id': that.data.m_live_data['camera_id'],
      'cur_page': that.data.m_cur_page
    }
    // 构造访问接口连接地址...
    var theUrl = g_app.globalData.m_urlPrev + 'Mini/getRecord'
    // 请求远程API过程...
    wx.request({
      url: theUrl,
      method: 'POST',
      data: thePostData,
      dataType: 'x-www-form-urlencoded',
      header: { 'content-type': 'application/x-www-form-urlencoded' },
      success: function (res) {
        // 调用接口失败...
        if (res.statusCode != 200) {
          that.setData({ m_show_more: false, m_no_more: '获取录像记录失败' })
          return
        }
        // 注意：这里不要调用 wx.hideLoading() ...
        // dataType 没有设置json，需要自己转换...
        var arrData = JSON.parse(res.data);
        // 获取录像失败的处理 => 显示获取到的错误信息...
        if( arrData.err_code > 0 ) {
          that.setData({ m_show_more: false, m_no_more: arrData.err_msg })
          return
        }
        // 获取到的记录数据不为空时才进行记录合并处理 => concat 不会改动原数据
        if ( (arrData.record instanceof Array) && (arrData.record.length > 0) ) {
          that.data.m_arrRecord = that.data.m_arrRecord.concat(arrData.record)
        }
        // 保存获取到的记录总数和总页数...
        that.data.m_total_num = arrData.total_num
        that.data.m_max_page = arrData.max_page
        // 如果到达最大页数，关闭加载更多信息...
        if (that.data.m_cur_page >= that.data.m_max_page) {
          that.setData({ m_show_more: false, m_no_more: '没有更多内容了' })
        }
        // 将数据显示到模版界面上去，并且显示加载更多页面...
        that.setData({ m_arrRecord: that.data.m_arrRecord })
      },
      fail: function (res) {
        // 注意：这里不要调用 wx.hideLoading() ...
        that.setData({ m_show_more: false, m_no_more: '获取录像记录失败' })
      }
    })
  },
  // 调用网站API获取直播通道地址信息...
  doAPIGetLiveAddr: function() {
    // 保存this对象...
    var that = this    
    // 准备需要的参数信息...
    var thePostData = {
      'node_proto': that.data.m_live_data['node_proto'],
      'node_addr': that.data.m_live_data['node_addr'],
      'camera_id': that.data.m_live_data['camera_id']
    }
    // 构造访问接口连接地址...
    var theUrl = g_app.globalData.m_urlPrev + 'Mini/getLiveAddr'
    // 请求远程API过程...
    wx.request({
      url: theUrl,
      method: 'POST',
      data: thePostData,
      dataType: 'x-www-form-urlencoded',
      header: { 'content-type': 'application/x-www-form-urlencoded' },
      success: function (res) {
        // 隐藏加载框...
        wx.hideLoading()
        // 调用接口失败...
        if( res.statusCode != 200 ) {
          that.doErrNotice()
          return
        }
        // dataType 没有设置json，需要自己转换...
        var arrData = JSON.parse(res.data);
        // 获取直播地址失败的处理...
        if (arrData.err_code > 0) {
          // 更新错误信息、错误提示，已经获取的直播信息...
          arrData.err_desc = ((typeof arrData.err_desc != 'undefined') ? arrData.err_desc : '请联系管理员，汇报错误信息。')
          that.setData({ m_is_error: true, 
                         m_err_msg: arrData.err_msg, 
                         m_err_desc: arrData.err_desc,
                         m_live_data: that.data.m_live_data })
          return
        }
        // 注意：这里是对象合并(Object.assign)，不是数组合并(concat)...
        // 将获取的通道地址数据与当前已有的通道数据合并...
        Object.assign(that.data.m_live_data, arrData)
      },
      fail: function (res) {
        // 隐藏加载框...
        wx.hideLoading()
        // 调用接口失败...
        that.doErrNotice()
      }
    })
  },
  // 接口失败的统一函数...
  doErrNotice: function() {
    this.setData({
      m_is_error: true,
      m_err_msg: '获取直播地址接口失败！',
      m_err_desc: '请联系管理员，汇报错误信息。',
      m_live_data: this.data.m_live_data
    })
  },
  // 下拉刷新事件...
  onPullDownRefresh: function () {
    // 首先，打印信息，停止刷新...
    console.log('onPullDownRefresh')
    wx.stopPullDownRefresh()
    // 如果不是直播的错误状态，直接返回...
    if( !this.data.m_is_error )
      return
    // 弹出等待框，获取直播地址...
    wx.showLoading({ title: '加载中' })
    // 获取通道的直播播放地址...
    this.doAPIGetLiveAddr()
  },
  // 页面上拉触底事件的处理函数...
  onReachBottom: function () {
    console.log('onReachBottom')
    // 如果到达最大页数，关闭加载更多信息...
    if (this.data.m_cur_page >= this.data.m_max_page) {
      this.setData({ m_show_more: false, m_no_more: '没有更多内容了' })
      return
    }
    // 没有达到最大页数，累加当前页码，请求更多数据...
    this.data.m_cur_page += 1
    this.doAPIGetRecord()
  },
  // 处理截图加载失败的事件 => 必须使用传递参数...
  doErrSnap: function (inEvent) {
    // 得到发生错误的条目编号，并修改成空值，让它切换到默认图片内容...
    var nItem = inEvent.target.dataset.errImg
    this.data.m_arrRecord[nItem].image_fdfs = ''
    // 将修改了的数据更新到界面数据对象当中 => 将空值图片自动设置成默认截图...
    this.setData({ m_arrRecord: this.data.m_arrRecord })
  },
  // 用户点击右上角分享...
  /*onShareAppMessage: function () {
    var that = this
    that.setData({ m_is_share: true })
    setTimeout(function(){
      that.setData({ m_is_share: false })
    }, 1000)
  },*/
  // 用户点击切换按钮的事件...
  onSwitchLive: function() {
    var that = this
    that.setData({ m_is_switch: true })
    setTimeout(function () {
      that.setData({ m_is_switch: false })
    }, 1000)
  }
})