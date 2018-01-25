// pages/gather/gather.js
// 获取全局的app对象...
const g_app = getApp()
Page({
  // 页面的初始数据
  data: {
    m_show_feed: false,
    m_arrGather: [],
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
      }
    ]
  },
  // 生命周期函数--监听页面加载
  onLoad: function (options) {
    // 如果没有获取到用户编号或用户信息，跳转到默认的授权页面，重新获取授权...
    if (g_app.globalData.m_nUserID <= 0 || g_app.globalData.m_userInfo == null) {
      wx.navigateTo({ url: '../default/default' })
      return
    }
    // 调用接口，获取属于当前用户的所有采集端列表 => 包含内网的采集端...
    this.doAPIGetGather(g_app.globalData.m_nUserID)  
  },
  // 获取属于当前用户的所有采集端列表 => 包含内网的采集端...
  doAPIGetGather(inUserID) {
    // 获取到的用户信息有效，弹出等待框...
    wx.showLoading({ title: '加载中' })
    // 显示导航栏加载动画...
    wx.showNavigationBarLoading()
    // 保存this对象...
    var that = this;
    // 构造访问接口连接地址 => 需要全部的采集端，包括内网的采集端...
    var theUrl = g_app.globalData.m_urlPrev + 'Mini/getGather/full/1/user_id/' + inUserID
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
        // dataType 默认json，不需要自己转换，直接替换新数据...
        // 反馈的结果失败 => 打印失败结果...
        if (res.data.err_code > 0) {
          console.log('doAPIGetGather error => ' + res.data.err_msg)
          that.setData({ m_show_feed: false })
          return
        }
        // 如果没有采集端记录，显示提示信息...
        if (!(res.data.list instanceof Array) || res.data.list.length <= 0) {
          that.setData({ m_show_feed: false })
          return
        }
        // 将获取到的采集端列表保存起来，并更新到界面当中...
        that.setData({ m_show_feed: true, m_arrGather: res.data.list })
      },
      fail: function (res) {
        wx.hideLoading()
        wx.hideNavigationBarLoading()
      }
    })
  },
  // 用户点击扫码按钮...
  doScanCode: function() {
    wx.scanCode({
      onlyFromCamera: true,
      success: (res) => {
        // 打印扫描结果...
        console.log(res)
        // 如果路径有效，直接跳转到相关页面 => path 是否带参数还不确定...
        if (typeof res.path != 'undefined' && res.path.length > 0) {
          res.path = '../../' + res.path
          wx.redirectTo({ url: res.path })
        }
      },
      fail: (res) => {
        console.log(res)
      }
    })
  },
  // 页面相关事件处理函数--监听用户下拉动作
  onPullDownRefresh: function () {
    // 首先，停止刷新...
    wx.stopPullDownRefresh()
    // 调用接口，获取属于当前用户的所有采集端列表 => 包含内网的采集端...
    this.doAPIGetGather(g_app.globalData.m_nUserID)  
  },
  // 用户点击查看采集端详情...
  doUnbind: function (inEvent) {
    // 准备采集端编号、用户编号...
    var theGatherID = inEvent.currentTarget.dataset.gather
    var theUserID = g_app.globalData.m_nUserID
    var that = this
    // 弹出确认框，是否真的要解除绑定...
    wx.showModal({
      title: '确认解绑',
      content: '确实要解除绑定吗？',
      success: function (res) {
        if (res.confirm) {
          that.doAPIUnbind(theUserID, theGatherID)
        }
      }
    })
  },
  // 执行解除绑定的API接口函数...
  doAPIUnbind: function(inUserID, inGatherID) {
    // 显示导航栏加载动画...
    wx.showNavigationBarLoading()
    // 保存this对象...
    var that = this;
    // 准备需要的参数信息...
    var thePostData = {
      'user_id': inUserID,
      'gather_id': inGatherID
    }
    // 构造访问接口连接地址 => 需要全部的采集端，包括内网的采集端...
    var theUrl = g_app.globalData.m_urlPrev + 'Mini/unbindGather'
    // 请求远程API过程 => 不管对错，直接更新...
    wx.request({
      url: theUrl,
      method: 'POST',
      data: thePostData,
      dataType: 'x-www-form-urlencoded',
      header: { 'content-type': 'application/x-www-form-urlencoded' },
      success: function (res) {
        wx.hideNavigationBarLoading()
        that.doAPIGetGather(inUserID)
      },
      fail: function (res) {
        wx.hideNavigationBarLoading()
      }
    })    
  }
})