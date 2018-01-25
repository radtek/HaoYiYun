// pages/private/private.js
// 加载模版需要的脚本...
var ZanTab = require("../../template/zan-tab.js")
var ZanSwitch = require("../../template/zan-switch.js")
// 获取全局的app对象...
const g_app = getApp()
// 本页面的代码 => 这里新增了扩展内容...
Page(Object.assign({}, ZanTab, ZanSwitch, {
  // 页面的初始数据
  data: {
    m_steps: [
      {
        done: false,
        current: true,
        text: '打开 https://myhaoyi.com，安装“服务器”'
      },
      {
        done: false,
        current: true,
        text: '打开 https://myhaoyi.com，安装“采集端”'
      },
      {
        done: false,
        current: true,
        text: '启动“采集端”，工具栏点击“绑定小程序”'
      },
      {
        done: false,
        current: true,
        text: '在“采集端”中，快速创建并发布通道'
      },
      {
        done: false,
        current: true,
        text: '注意：小程序暂时不能查看局域网内的通道'
      }
    ],
    m_gather: {
      list: [],
      height: 44,
      scroll: true,
      selectedId: -1,
      selectedItem: null
    },
    m_show_feed: true,
    m_show_more: true,
    m_show_init: false,
    m_arrCamera: [],
    m_total_num: 0,
    m_cur_page: 1,
    m_max_page: 1
  },
  // 响应共享按钮切换事件...
  handleZanSwitchChange(e) {
    // 发生共享通道变化，设置重载标志...
    g_app.globalData.m_bLoadShare = true
    // 获取索引编号和切换状态...
    var componentId = e.componentId;
    var theShared = e.checked ? 1 : 0;
    // 设置正在加载标志，并应用到界面上...
    this.data.m_arrCamera[componentId].loading = true
    this.setData({ m_arrCamera: this.data.m_arrCamera })
    // 调用API接口保存共享状态到远程服务器...
    var that = this
    // 准备需要的参数信息...
    var thePostData = {
      'shared': theShared,
      'user_id': g_app.globalData.m_nUserID,
      'node_id': that.data.m_gather.selectedItem.node_id,
      'node_addr': that.data.m_gather.selectedItem.node_addr,
      'node_proto': that.data.m_gather.selectedItem.node_proto,
      'camera_id': that.data.m_arrCamera[componentId].camera_id
    }
    // 构造访问接口连接地址...
    var theUrl = g_app.globalData.m_urlPrev + 'Mini/saveShare'
    // 请求远程API过程...
    wx.request({
      url: theUrl,
      method: 'POST',
      data: thePostData,
      dataType: 'x-www-form-urlencoded',
      header: { 'content-type': 'application/x-www-form-urlencoded' },
      success: function (res) {
        // 修改加载状态，并应用到界面上...
        that.data.m_arrCamera[componentId].loading = false
        do {
          // 调用接口失败，打印错误信息...
          if (res.statusCode != 200) {
            console.log('SaveShare error => ' + res.statusCode)
            break
          }
          // dataType 没有设置json，需要自己转换...
          var arrData = JSON.parse(res.data);
          // 保存状态失败，打印错误信息...
          if (arrData.err_code > 0) {
            console.log('SaveShare error => ' + arrData.err_msg)
            break
          }
          // 保存最终的共享状态到数组队列当中...
          that.data.m_arrCamera[componentId].shared = theShared
        } while( false )
        // 将最终的结果反应到界面上...
        that.setData({ m_arrCamera: that.data.m_arrCamera })
      },
      fail: function(res) {
        // 修改加载状态，并应用到界面上...
        that.data.m_arrCamera[componentId].loading = false
        that.setData({ m_arrCamera: that.data.m_arrCamera })
      }
    })
  },
  // 响应标签切换事件...
  handleZanTabChange(e) {
    // 获取传递过来的参数信息...
    var componentId = e.componentId;
    var selectedItem = e.selectedItem;
    // 如果点击对象就是当前选中对象 => 直接返回...
    if (this.data.m_gather.selectedId == selectedItem.gather_id)
      return
    // 整个保存选中的采集端对象 => 获取通道列表使用...
    this.data.m_gather.selectedItem = selectedItem
    // 获取到的用户信息有效，弹出等待框...
    wx.showLoading({ title: '加载中' })
    // 其它变量还原，需要更新到显示页面 => 可以避免画面闪烁...
    this.setData({
      m_cur_page: 1,
      m_max_page: 1,
      m_show_feed: true,
      m_show_more: true,
      m_show_init: false,
      m_arrCamera: [],
      [`${componentId}.selectedId`]: selectedItem.gather_id
    })
    // 获取选中采集端下面所有的通道列表...
    this.doAPIGetCamera(selectedItem)
  },
  // 生命周期函数--监听页面加载
  onLoad: function (options) {
    // 如果没有获取到用户编号或用户信息，跳转到默认的授权页面，重新获取授权...
    if (g_app.globalData.m_nUserID <= 0 || g_app.globalData.m_userInfo == null) {
      wx.navigateTo({ url: '../default/default' })
      return
    }
    // 调用接口，获取属于当前用户的所有采集端列表...
    this.doAPIGetGather(g_app.globalData.m_nUserID)
  },
  // 获取属于当前用户的所有采集端列表...
  doAPIGetGather(inUserID) {
    // 获取到的用户信息有效，弹出等待框...
    wx.showLoading({ title: '加载中' })
    // 显示导航栏加载动画...
    wx.showNavigationBarLoading()
    // 保存this对象...
    var that = this;
    // 构造访问接口连接地址...
    var theUrl = g_app.globalData.m_urlPrev + 'Mini/getGather/user_id/' + inUserID
    // 请求远程API过程...
    wx.request({
      url: theUrl,
      method: 'GET',
      success: function (res) {
        // 隐藏加载框...
        wx.hideLoading()
        wx.hideNavigationBarLoading()
        // 调用接口失败 => 直接返回...
        if (res.statusCode != 200) {
          console.log('doAPIGetGather error => ' + res.statusCode)
          that.setData({ m_show_feed: false })
          return
        }
        // 反馈的结果失败 => 打印失败结果...
        if (res.data.err_code > 0) {
          console.log('doAPIGetGather error => ' + res.data.err_msg)
          that.setData({ m_show_feed: false })
          return
        }
        // dataType 默认json，不需要自己转换，直接替换新数据...
        that.data.m_gather.list = []
        that.data.m_gather.selectedId = -1
        that.data.m_gather.selectedItem = null
        // 如果反馈的列表是数组才进行存储...
        // 如果没有采集端记录，显示提示信息...
        if (!(res.data.list instanceof Array) || res.data.list.length <= 0) {
          that.setData({ m_show_feed: false })
          return
        }
        // 获取到的采集端信息有效，弹出等待框...
        wx.showLoading({ title: '加载中' })
        // 将获取到的采集端列表保存起来，并更新到界面当中...
        that.data.m_gather.list = res.data.list
        that.data.m_gather.selectedItem = res.data.list[0]
        that.data.m_gather.selectedId = res.data.list[0].gather_id
        that.setData({ m_gather: that.data.m_gather })
        // 调用API获取选中采集端下所有的通道列表...
        that.doAPIGetCamera(res.data.list[0])
      },
      fail: function (res) {
        // 隐藏加载框...
        wx.hideLoading()
        wx.hideNavigationBarLoading()
        // 没有通道数据，显示警告框页面...
        if (that.data.m_arrCamera.length <= 0) {
          that.setData({ m_show_feed: false })
        }
      }
    })
  },
  // 获取指定采集端下所有的通道列表...
  doAPIGetCamera: function(inGatherItem) {
    // 显示导航栏加载动画...
    wx.showNavigationBarLoading()
    // 保存this对象...
    var that = this;
    // 准备需要的参数信息...
    var thePostData = {
      'node_proto': inGatherItem.node_proto,
      'node_addr': inGatherItem.node_addr,
      'mac_addr': inGatherItem.mac_addr,
      'cur_page': that.data.m_cur_page
    }
    // 构造访问接口连接地址...
    var theUrl = g_app.globalData.m_urlPrev + 'Mini/getCamera'
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
        wx.hideNavigationBarLoading()
        do {
          // 调用接口失败 => 直接返回...
          if (res.statusCode != 200) {
            console.log('doAPIGetCamera error => ' + res.statusCode)
            break
          }
          // dataType 没有设置json，需要自己转换...
          var arrData = JSON.parse(res.data);
          // 反馈的结果失败 => 打印失败结果...
          if (arrData.err_code > 0) {
            console.log('doAPIGetCamera error => ' + arrData.err_msg)
            break
          }
          // 保存获取到的记录总数和总页数...
          that.data.m_total_num = arrData.total_num
          that.data.m_max_page = arrData.max_page
          // 获取到的记录数据为空时，显示提示信息...
          if (!(arrData.camera instanceof Array) || arrData.camera.length <= 0) {
            break;
          }
          // 获取到的记录数据不为空时才进行记录合并处理 => concat 不会改动原数据
          if ((arrData.camera instanceof Array) && (arrData.camera.length > 0)) {
            that.data.m_arrCamera = that.data.m_arrCamera.concat(arrData.camera)
          }
        } while( false )
        // 如果当前采集端的通道记录总数为0，显示提示列表信息...
        if (that.data.m_arrCamera.length <= 0 || that.data.m_max_page <= 0 ) {
          that.setData({ m_show_feed: false })
          return
        }
        // 如果到达最大页数，关闭加载更多信息...
        if (that.data.m_cur_page >= that.data.m_max_page) {
          that.setData({ m_show_more: false })
        }
        // 将数据显示到模版界面上去，并且显示加载更多页面...
        that.setData({ m_arrCamera: that.data.m_arrCamera, m_show_init: true })
      },
      fail: function (res) {
        wx.hideLoading()
        wx.hideNavigationBarLoading()
      }
    })
  },
  // 生命周期函数--监听页面卸载
  onUnload: function () {
  },
  // 页面相关事件处理函数--监听用户下拉动作
  onPullDownRefresh: function () {
    // 首先，打印信息，停止刷新...
    console.log('onPullDownRefresh')
    wx.stopPullDownRefresh()
    // 将通道列表还原，但不更新到显示页面 => 可以避免画面闪烁...
    this.data.m_arrCamera = []
    // 其它变量还原，需要更新到显示页面 => 可以避免画面闪烁...
    this.setData({ m_cur_page: 1, m_max_page: 1, m_show_feed: true, 
                   m_show_more: true, m_show_init: false })
    // 调用接口，获取属于当前用户的所有采集端列表...
    this.doAPIGetGather(g_app.globalData.m_nUserID)
  },
  // 页面上拉触底事件的处理函数
  onReachBottom: function () {
    console.log('onReachBottom')
    // 如果到达最大页数，关闭加载更多信息...
    if (this.data.m_cur_page >= this.data.m_max_page) {
      this.setData({ m_show_more: false })
      return
    }
    // 没有达到最大页数，累加当前页码，请求更多数据...
    var theCurGather = this.data.m_gather.selectedItem
    this.data.m_cur_page += 1
    this.doAPIGetCamera(theCurGather)
  },
  // 响应用户点击单条记录事件...
  doTapItem: function (inEvent) {
    // 获取被点击通道的原始数据内容...
    var theUserInfo = g_app.globalData.m_userInfo
    var theCurGather = this.data.m_gather.selectedItem
    var theItem = inEvent.currentTarget.dataset.track
    var theLiveUrl = '../live/live?type=1&data='
    // 追加直播播放页面需要的节点内容...
    theItem.node_proto = theCurGather.node_proto
    theItem.node_addr = theCurGather.node_addr
    // 追加直播播放页面需要的用户信息...
    theItem.wx_headurl = theUserInfo.avatarUrl.replace(/\/0/,'/96')
    theItem.wx_nickname = theUserInfo.nickName
    // 将数据json化，跳转到直播播放页面...
    theLiveUrl += JSON.stringify(theItem)
    wx.navigateTo({ url: theLiveUrl })
  }
}))