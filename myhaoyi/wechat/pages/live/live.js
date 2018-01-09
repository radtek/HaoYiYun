// pages/live/live.js
// 加载模版需要的脚本...
var ZanToast = require("../../template/zan-toast.js")
// 定义播放状态 => 直播点播都适用...
const PLAY_LOADING  = 0
const PLAY_ERROR    = 1
const PLAY_RUN      = 2
// 定义直播类型...
// 0 => 共享通道...
// 1 => 个人通道...
const LIVE_SHARE  = 0
const LIVE_PERSON = 1
// 获取全局的app对象...
const g_app = getApp()
// 本页面的代码 => 这里新增了扩展内容...
Page(Object.assign({}, ZanToast, {
  // 页面的初始数据...
  data: {
    m_cur_page: 1,
    m_max_page: 1,
    m_total_num: 0,
    m_arrRecord: [],
    m_is_live: true,
    m_show_more: true,
    m_vod_data: null,
    m_live_data: null,
    m_live_player: null,
    m_live_type: LIVE_SHARE,
    m_play_state: PLAY_LOADING,
    m_live_objectFit: "contain",
    m_live_orientation: "vertical",
    m_live_is_fullscreen: false,
    m_live_is_bindstate: false,
    m_live_is_paused: false,
    m_live_poster_height: 210,
    m_live_show_control: false,
    m_live_show_snap: true,
    m_live_state_timer: -1,
    m_live_click_timer: -1,
    m_live_clock_verify: -1
  },
  // 生命周期函数--监听页面加载...
  onLoad: function (options) {
    // 这里获取的 m_live_data 是对象，不是数组...
    this.data.m_live_type = parseInt(options.type)
    this.data.m_live_data = JSON.parse(options.data)
    // 传递的信息有效，弹出等待框...
    wx.showLoading({ title: '加载中' })
    // 获取通道的直播播放地址...
    this.doAPIGetLiveAddr()
    // 获取当前通道下相关的录像列表...
    this.doAPIGetRecord()
  },
  // 生命周期函数--监听页面卸载
  onUnload: function () {
    // 无论什么状态，都直接关闭直播状态汇报时钟...
    this.resetLiveClock()
  },
  // 重置直播汇报时钟...
  resetLiveClock() {
    clearInterval(this.data.m_live_clock_verify)
    this.data.m_live_clock_verify = -1
  },
  // 创建直播汇报时钟 => 每隔15秒调用ajax通知接口...
  buildLiveClock() {
    // 创建时钟之前，先关闭之前的时钟...
    this.resetLiveClock()
    // 创建新的时钟...
    var that = this
    that.data.m_live_clock_verify = setInterval(function () {
      that.doAPILiveVerify(1)
    }, 15000)
  },
  // 汇报直播在线状态 => inPlayActive不能用true或false，必须用1或0...
  doAPILiveVerify: function(inPlayActive) {
    // 保存this对象...
    var that = this
    // 准备需要的参数信息...
    // player_type => 0(rtmp) 1(html5)
    var thePostData = {
      'node_proto': that.data.m_live_data['node_proto'],
      'node_addr': that.data.m_live_data['node_addr'],
      'rtmp_live': that.data.m_live_data['camera_id'],
      'player_id': that.data.m_live_data['player_id'],
      'player_active': inPlayActive,
      'player_type': 0
    }
    // 构造访问接口连接地址...
    var theUrl = g_app.globalData.m_urlPrev + 'Mini/liveVerify'
    // 请求远程API过程...
    wx.request({
      url: theUrl,
      method: 'POST',
      data: thePostData,
      dataType: 'x-www-form-urlencoded',
      header: { 'content-type': 'application/x-www-form-urlencoded' },
      success: function (res) {
        // 调用接口失败 => 关闭汇报时钟...
        if (res.statusCode != 200) {
          that.resetLiveClock()
          return
        }
        // dataType 没有设置json，需要自己转换...
        var arrData = JSON.parse(res.data);
        // 汇报反馈失败的处理 => 关闭时钟...
        if (arrData.err_code > 0) {
          console.log('Live-Verify error => ' + arrData.err_msg)
          that.resetLiveClock()
          return
        }
        // 汇报直播状态成功 => 打印信息...
        console.log(thePostData)
      },
      fail: function (res) {
        console.log('Live-Verify error => fail')
        that.resetLiveClock()
      }
    })
  },
  // 调用网站API获取直播通道地址信息...
  doAPIGetLiveAddr: function() {
    // 立即重置状态标志...
    this.data.m_live_is_bindstate = false
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
          that.setData({ m_play_state: PLAY_ERROR, 
                         m_err_msg: arrData.err_msg, 
                         m_err_desc: arrData.err_desc,
                         m_live_data: that.data.m_live_data })
          return
        }
        // 再次启动等待框，等待直播反馈之后再关闭...
        wx.showLoading({ title: '正在连接...' })
        // 注意：这里是对象合并(Object.assign)，不是数组合并(concat)...
        // 将获取的通道地址数据与当前已有的通道数据合并...
        Object.assign(that.data.m_live_data, arrData)
        // 将数据应用到界面当中，修改状态为 PLAY_RUN ...
        that.setData({ m_is_live: true,
                       m_play_state: PLAY_RUN,
                       m_live_data: that.data.m_live_data })
        // <live-player>组件与操作对象相互关联起来...
        // 注意：组件对象只创建一次，不要重复创建...
        if (!that.data.m_live_player) {
          that.data.m_live_player = wx.createLivePlayerContext("myLivePlayer")
        }
        // 启动汇报状态时钟...
        that.buildLiveClock()
        // 启动检测 onLiveStateChange 是否有效绑定...
        // 注意：5秒未收到2004播放状态，失败关闭...
        that.data.m_live_state_timer = setTimeout( function() {
          that.doCheckLiveState()
        }, 5000)
      },
      fail: function (res) {
        // 隐藏加载框...
        wx.hideLoading()
        // 调用接口失败...
        that.doErrNotice()
      }
    })
  },
  // 调用网站API获取通道下的录像列表...
  doAPIGetRecord: function () {
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
        if (arrData.err_code > 0) {
          that.setData({ m_show_more: false, m_no_more: arrData.err_msg })
          return
        }
        // 获取到的记录数据不为空时才进行记录合并处理 => concat 不会改动原数据
        if ((arrData.record instanceof Array) && (arrData.record.length > 0)) {
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
  // 调用网站API保存点击次数 => 远程自动累加...
  doAPISaveClick: function(inRecIndex) {
    // 判断输入参数是否有效...
    if ( inRecIndex < 0 || inRecIndex >= this.data.m_arrRecord.length)
      return
    // 显示导航栏加载动画...
    wx.showNavigationBarLoading()
    // 保存this对象...
    var that = this
    // 这里是引用，只要改变theItem，所有都变...
    var theItem = that.data.m_arrRecord[inRecIndex]
    // 准备需要的参数信息...
    var thePostData = {
      'node_proto': that.data.m_live_data['node_proto'],
      'node_addr': that.data.m_live_data['node_addr'],
      'record_id': theItem.record_id
    }
    // 构造访问接口连接地址...
    var theUrl = g_app.globalData.m_urlPrev + 'Mini/saveClick'
    // 调用API远程累加点击计数器 => 返回累加后的点击次数...
    wx.request({
      url: theUrl,
      method: 'POST',
      data: thePostData,
      dataType: 'x-www-form-urlencoded',
      header: { 'content-type': 'application/x-www-form-urlencoded' },
      success: function (res) {
        // 隐藏加载框...
        wx.hideNavigationBarLoading()
        // 调用接口失败 => 关闭汇报时钟...
        if (res.statusCode != 200) {
          console.log('SaveClick error => ' + res.statusCode)
          return
        }
        // dataType 没有设置json，需要自己转换...
        var arrData = JSON.parse(res.data);
        // 获取数据失败，直接返回...
        if (arrData.err_code > 0) {
          console.log('SaveClick error => ' + arrData.err_msg)
          return
        }
        // 保存点击次数到本地数据当中...
        theItem.clicks = arrData.clicks
        that.data.m_vod_data.clicks = arrData.clicks
        // 更新点击次数到界面当中 => 播放列表，焦点对象 都需要同时更新...
        that.setData({ m_arrRecord: that.data.m_arrRecord, m_vod_data: that.data.m_vod_data })
      },
      fail: function (res) {
        // 隐藏加载框...
        wx.hideNavigationBarLoading()
      }
    })
  },
  // <live-player>状态事件绑定失败的处理 => 5秒未收到2004播放状态，失败关闭...
  doCheckLiveState: function() {
    // 保存this对象...
    var that = this
    // 如果已经有状态变化，直接返回...
    if (that.data.m_live_is_bindstate)
      return
    // 打印绑定错误信息...
    console.log('LiveState error => 2004 event not come in')
    // 关闭连接等待框...
    wx.hideLoading()
    // 调用接口停止直播...
    that.data.m_live_player.stop()
    // 设置为暂停状态，显示快照图片层 => 先设置状态，再调用接口...
    that.setData({ m_live_show_snap: true, m_live_is_paused: true })
    // 不能直接显示(有可能显示不全)，需要使用时钟延时显示...
    setTimeout(function () {
      that.showZanToast('已经断开，停止播放')
    }, 100)
  },
  // 处理<live-player>全屏状态变化通知事件...
  /*onLiveFullChange: function(inEvent) {
    console.log(inEvent)
  },*/
  // 处理<live-player>状态变化通知事件...
  onLiveStateChange: function(inEvent) {
    // 临时存放反馈的错误码信息...
    var theCode = inEvent.detail.code
    console.log(inEvent.detail.code)
    // 开始正常播放，关闭等待框 => 设定播放状态，关闭快照显示层...
    if (theCode == 2004) {
      this.setData({ m_live_is_bindstate: true, 
                     m_live_show_snap: false, 
                     m_live_is_paused: false })
      wx.hideLoading()
      return
    }
    // 如果出现链接断开的情况 => 显示快照，弹框提示...
    if (theCode == 2103) {
      // 先关闭之间的等待框...
      wx.hideLoading()
      // 整个模拟重载页面...
      this.onSwitchLive()
      /*// 重置状态更新时钟...
      this.resetLiveClock()
      // 再开启新的重连框，显示重连页面...
      wx.showLoading({title: '正在重连...'})
      this.setData({ m_play_state: PLAY_LOADING })
      // 获取通道的直播播放地址...
      this.doAPIGetLiveAddr()*/
      return
    }
    // 处理不同的错误情况 => 弹框说明，停止播放，还原状态...
    var theErrMsg = null
    switch( theCode ) {
      case -2301: theErrMsg = '多次重连失败，停止播放'; break;
      case  2006: theErrMsg = '视频播放结束，停止播放'; break;
      case  3001: theErrMsg = 'DNS解析失败，停止播放'; break;
      case  3002: theErrMsg = '服务器连接失败，停止播放'; break;
      case  3003: theErrMsg = '服务器握手失败，停止播放'; break;
      default:    theErrMsg = null; break;
    }
    // 多次重连失败 => 复位时钟，让中转器超时删除直播播放器...
    if (theCode == -2301) {
      this.resetLiveClock()
    }
    // 如果发生了验证错误，弹框说明，停止播放，还原状态...
    if (theErrMsg != null ) {
      wx.hideLoading()
      this.data.m_live_player.stop()
      this.setData({ m_live_is_paused: true })
      this.showZanToast(theErrMsg)
    }
  },
  // 点击直播视图区域...
  doClickLiveArea: function() {
    // 保存this对象...
    var that = this
    // 首先，关闭之间创建的超时时钟，并将时钟变量还原...
    clearTimeout(that.data.m_live_click_timer)
    that.data.m_live_click_timer = -1
    // 根据状态标志进行不同的处理...
    if (that.data.m_live_show_control) {
      // 如果正在显示控制条，立即停止显示...
      that.setData({ m_live_show_control: false })
    } else {
      // 如果没有显示控制，立即显示控制条...
      that.setData({ m_live_show_control: true })
      // 在5秒之后自动停止显示控制条...
      that.data.m_live_click_timer = setTimeout(function () {
        that.setData({ m_live_show_control: false })
      }, 5000)
    }
  },
  // 点击直播播放按钮...
  doClickPlay: function() {
    // 保存this对象...
    var that = this
    // 如果直播对象无效，直接返回...
    if (!that.data.m_live_player) {
      console.log('live player is null')
      return
    }
    // 注意：这里先设置状态，再调用接口，目的是为了快速响应用户点击...
    if (that.data.m_live_is_paused) {
      // 开启连接等待框...
      wx.showLoading({ title: '正在连接...' })
      // 设置为启动状态 => 先设置状态，再调用接口...
      // 注意：这里没有强制显示快照层，防止闪烁...
      that.setData({ m_live_is_bindstate: false, m_live_is_paused: false })
      // 调用接口启动直播...
      that.data.m_live_player.play()
      // 启动检测 onLiveStateChange 是否有效绑定...
      // 注意：5秒未收到2004播放状态，失败关闭...
      that.data.m_live_state_timer = setTimeout(function () {
        that.doCheckLiveState()
      }, 5000)
    } else {
      // 首先，关闭之间创建的超时时钟，并将时钟变量还原 => 避免重复调用...
      clearTimeout(that.data.m_live_state_timer)
      that.data.m_live_state_timer = -1
      // 设置为暂停状态，显示快照图片层 => 先设置状态，再调用接口...
      that.setData({ m_live_show_snap: true, m_live_is_paused: true })
      // 调用接口停止直播...
      that.data.m_live_player.stop()
      // 关闭连接等待框...
      wx.hideLoading()
      // 全屏状态下直接返回 => 全屏下会自动重连...
      if (that.data.m_live_is_fullscreen)
        return
      // 非全屏状态，显示一个提示框 => 显得不那么突然...
      // 不能直接显示(有可能显示不全)，需要使用时钟延时显示...
      setTimeout( function() {
        that.showZanToast('已经断开，停止播放')
      }, 100)
      // 这里不要关闭汇报时钟，再次点击还能正常播放...
      // 但是，这里是rtmp，srs会立即中断连接，用户数为0时，会通知采集端停止推流...
      // 因此，应该将srs用户数为0的汇报通知去掉，完全由时钟去处理用户的在线状态...
    }
  },
  // 点击直播全屏按钮...
  doClickFull: function() {
    // 如果直播对象无效，直接返回...
    if (!this.data.m_live_player) {
      console.log('live player is null')
      return
    }
    // 如果是全屏则还原，是窗口则全屏...
    if (this.data.m_live_is_fullscreen) {
      this.data.m_live_player.exitFullScreen()
      this.setData({ m_live_poster_height: 210, m_live_is_fullscreen: false })
    } else {
      var posterHeight = wx.getSystemInfoSync().screenWidth
      this.data.m_live_player.requestFullScreen({ direction: 90 })
      this.setData({ m_live_poster_height: posterHeight, m_live_is_fullscreen: true })
    }
  },
  // 接口失败的统一函数...
  doErrNotice: function() {
    this.setData({
      m_play_state: PLAY_ERROR,
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
    // 是直播，并且有错误，才刷新，再次获取直播地址...
    if ( this.data.m_is_live && this.data.m_play_state == PLAY_ERROR ) {
      // 还原加载状态 => PLAY_LOADING...
      this.setData({ m_play_state: PLAY_LOADING })
      // 弹出等待框，获取直播地址...
      wx.showLoading({ title: '加载中' })
      // 获取通道的直播播放地址...
      this.doAPIGetLiveAddr()
    }
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
  // 响应用户点击单条录像记录事件...
  doTapRecord: function (inEvent) {
    // 改变标题名称 => 录像 - 播放...
    wx.setNavigationBarTitle({ title: '录像 - 播放' })
    // 复位时钟，让中转器超时删除直播播放器...
    this.resetLiveClock()
    // 切换到点播模式，保存点播配置，关闭错误提示框 => 焦点记录设定放在了wxml当中...
    var theItem = inEvent.currentTarget.dataset.record
    var theIndex = inEvent.currentTarget.dataset.index
    // 调用API远程累加点击计数器...
    this.doAPISaveClick(theIndex)
    // 设置点播状态、运行状态、焦点对象...
    this.setData({ m_play_state: PLAY_RUN, m_is_live: false, m_vod_data: theItem })
  },
  // 响应播放完毕的事件通知...
  doPlayEnded: function(inEvent) {
    // 如果是直播状态模式，直接返回...
    if( this.data.m_is_live )
      return
    // 利用当前点播播放焦点记录编号在现有数据列表中查找索引...
    var theFocusID = this.data.m_vod_data.record_id
    for (var i = 0; i < this.data.m_arrRecord.length; ++i) {
      var theCurID = this.data.m_arrRecord[i].record_id
      if (theFocusID == theCurID) {
        // 累加焦点索引编号...
        var theNewIndex = i + 1
        // 索引编号越界...
        if (theNewIndex >= this.data.m_arrRecord.length) {
          theNewIndex = 0
        }
        // 调用API远程累加点击计数器...
        this.doAPISaveClick(theNewIndex)
        // 模拟点击对应的索引编号...
        this.setData({m_vod_data: this.data.m_arrRecord[theNewIndex]})
        return
      }
    }
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
    // 保存this对象...
    var that = this
    // 复位时钟，让中转器超时删除直播播放器...
    that.resetLiveClock()
    // 设置切换动画 => 800毫秒后还原...
    that.setData({ m_is_switch: true })
    setTimeout(function () {
      that.setData({ m_is_switch: false })
    }, 800)
    // 注意：不要阻止重连刷新，尽管重来...
    // 改变标题名称 => 直播 - 播放...
    wx.setNavigationBarTitle({ title: '直播 - 播放' })
    // 还原播放状态和点播数据 => 直播 | 加载中...
    that.setData({ m_play_state: PLAY_LOADING, m_is_live: true, m_vod_data: null })
    // 弹出等待框，获取直播地址...
    wx.showLoading({ title: '加载中' })
    // 获取通道的直播播放地址...
    that.doAPIGetLiveAddr()
  }
}))
