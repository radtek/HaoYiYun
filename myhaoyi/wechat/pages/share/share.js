// pages/share/share.js
// 获取全局的app对象...
const g_app = getApp()
// 本页面的代码...
Page({
  data: {
    m_cur_page: 1,
    m_max_page: 1,
    m_total_num: 0,
    m_arrTrack: [],
    m_show_feed: true,
    m_show_more: true,
    m_show_init: false
  },
  onLoad: function (options) {
    // 如果没有获取到用户编号或用户信息，跳转到默认的授权页面，重新获取授权...
    if( g_app.globalData.m_nUserID <= 0 || g_app.globalData.m_userInfo == null ) {
      wx.navigateTo({ url: '../default/default' })
      return
    }
    // 获取到的用户信息有效，弹出等待框...
    wx.showLoading({ title: '加载中' })
    // 调用接口，获取共享通道列表数据 => 默认获取第一页...
    this.doAPIGetShare(this.data.m_cur_page)
  },
  // 调用网站API获取共享通道列表...
  doAPIGetShare: function(inCurPage) {
    var that = this;
    // 构造访问接口连接地址...
    var theUrl = g_app.globalData.m_urlPrev + 'Mini/getShare/p/' + inCurPage
    // 请求远程API过程...
    wx.request({
      url: theUrl,
      method: 'GET',
      success: function (res) {
        // 隐藏加载框...
        wx.hideLoading()
        // 如果没有共享通道记录，显示提示信息...
        if ( (that.data.m_arrTrack.length <= 0) && (!(res.data.track instanceof Array) || res.data.track.length <= 0) ) {
          that.setData({ m_show_feed: false })
          return
        }
        // dataType 默认json，不需要自己转换，将数据进行累加，concat之后需要手动赋值...
        if( (res.data.track instanceof Array) && (res.data.track.length > 0) ) {
          that.data.m_arrTrack = that.data.m_arrTrack.concat(res.data.track)
        }
        // 保存获取到的记录总数和总页数...
        that.data.m_total_num = res.data.total_num
        that.data.m_max_page = res.data.max_page
        // 如果到达最大页数，关闭加载更多信息...
        if (that.data.m_cur_page >= that.data.m_max_page) {
          that.setData({ m_show_more: false })
        }
        // 将数据显示到模版界面上去，并且显示加载更多页面...
        that.setData({ m_arrTrack: that.data.m_arrTrack, m_show_init: true })
      },
      fail: function (res) {
        // 隐藏加载框...
        wx.hideLoading()
        // 没有通道数据，显示警告框页面...
        if ( that.data.m_arrTrack.length <= 0) {
          that.setData({ m_show_feed: false })
        }
      }
    })
  },
  // 下拉刷新事件...
  onPullDownRefresh: function() {
    // 首先，打印信息，停止刷新...
    console.log('onPullDownRefresh')
    wx.stopPullDownRefresh()
    // 获取到的用户信息有效，弹出等待框...
    wx.showLoading({ title: '加载中' })
    // 将通道列表还原，但不更新到显示页面 => 可以避免画面闪烁...
    this.data.m_arrTrack = []
    // 其它变量还原，需要更新到显示页面 => 可以避免画面闪烁...
    this.setData({ m_cur_page: 1,
                   m_max_page: 1, 
                   m_show_feed: true,
                   m_show_more: true,
                   m_show_init: false })
    // 重新刷新通道数据...
    this.doAPIGetShare(this.data.m_cur_page)
  },
  // 上拉触底事件...
  onReachBottom: function() {
    console.log('onReachBottom')
    // 如果到达最大页数，关闭加载更多信息...
    if( this.data.m_cur_page >= this.data.m_max_page ) {
      this.setData({ m_show_more: false })
      return
    }
    // 没有达到最大页数，累加当前页码，请求更多数据...
    this.data.m_cur_page += 1
    this.doAPIGetShare(this.data.m_cur_page)
  },
  // 处理截图加载失败的事件 => 必须使用传递参数...
  doErrSnap: function(inEvent) {
    // 得到发生错误的条目编号，并修改成空值，让它切换到默认图片内容...
    var nItem = inEvent.target.dataset.errImg
    this.data.m_arrTrack[nItem].image_fdfs = ''
    // 将修改了的数据更新到界面数据对象当中 => 将空值图片自动设置成默认截图...
    this.setData({ m_arrTrack: this.data.m_arrTrack })
  },
  /*// 响应用户点击发布者事件...
  catchtap可以阻止后续的冒泡...
  doTapOwner: function () {
    console.log('doOwner')
  },*/
  // 响应用户点击单条记录事件...
  doTapItem: function(event) {
    var theItem = event.currentTarget.dataset.track
    var theUrl = '../live/live?type=0&data='
    theUrl += JSON.stringify(theItem)
    wx.navigateTo({ url: theUrl })
  }
})