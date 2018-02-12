// pages/bind/bind.js
// 定义绑定操作子命令...
const BIND_SCAN = 1
const BIND_SAVE = 2
const BIND_CANCEL = 3
// 获取全局的app对象...
const g_app = getApp()
Page({
  // 页面的初始数据
  data: {
    m_strError: '',
    m_dbGather: null,
    m_show_feed: true
  },
  // 生命周期函数--监听页面加载
  onLoad: function (options) {
    // 判断是否是正常扫码跳转过来的页面 => 将获取到的采集端编号，直接保存到全局的数据区当中...
    if (typeof options.scene != 'undefined') {
      g_app.globalData.m_nBindGatherID = decodeURIComponent(options.scene)
    }
    // 如果获取的采集端编号无效，直接提示，返回...
    if (g_app.globalData.m_nBindGatherID <= 0) {
      var strError = '无效的采集端编号：' + g_app.globalData.m_nBindGatherID
      this.setData({ m_show_feed: false, m_strError: strError })
      return
    }
    // 获取到了正确的的采集端编号，判断用户编号是否正确，不正确，跳转到默认的授权页面，重新获取授权...
    // 注意：这里使用redirectTo，关闭当前页面，不会有回退按钮...
    if (g_app.globalData.m_nUserID <= 0 || g_app.globalData.m_userInfo == null) {
      wx.redirectTo({ url: '../default/default' })
      return
    }
    // 获取采集端详细信息...
    this.doAPIFindGather()
  },
  // 通过采集端编号获取采集端详情...
  doAPIFindGather: function () {
    // 获取到的用户信息有效，弹出等待框...
    wx.showLoading({ title: '加载中' })
    // 显示导航栏加载动画...
    wx.showNavigationBarLoading()
    // 保存this对象...
    var strError = ''
    var that = this
    var theGatherID = g_app.globalData.m_nBindGatherID
    // 构造访问接口连接地址 => 需要全部的采集端，包括内网的采集端...
    var theUrl = g_app.globalData.m_urlPrev + 'Mini/findGather/gather_id/' + theGatherID
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
          strError = '获取采集端信息失败，错误码：' + res.statusCode
          that.setData({ m_show_feed: false, m_strError: strError })
          return
        }
        // dataType 默认json，不需要自己转换，直接替换新数据...
        // 反馈的结果失败 => 打印失败结果...
        if (res.data.err_code > 0) {
          strError = res.data.err_msg + ' 错误码：' + res.data.err_code
          that.setData({ m_show_feed: false, m_strError: strError })
          return
        }
        // 将获取到的采集端数据应用到界面上去...
        that.setData({ m_dbGather: res.data.dbGather })
        // 向采集端发送扫码成功子命令...
        that.doAPIBindMini(BIND_SCAN)
      },
      fail: function (res) {
        wx.hideLoading()
        wx.hideNavigationBarLoading()
      }
    })
  },
  // 转发绑定子命令到对应的采集端...
  doAPIBindMini: function (inSubCmd) {
    // 获取到的用户信息有效，弹出等待框...
    wx.showLoading({ title: '加载中' })
    // 显示导航栏加载动画...
    wx.showNavigationBarLoading()
    // 保存this对象...
    var strError = ''
    var that = this
    // 准备需要的参数信息...
    var thePostData = {
      'node_proto': that.data.m_dbGather.node_proto,
      'node_addr': that.data.m_dbGather.node_addr,
      'gather_id': that.data.m_dbGather.gather_id,
      'mac_addr': that.data.m_dbGather.mac_addr,
      'user_id': g_app.globalData.m_nUserID,
      'bind_cmd': inSubCmd
    }
    // 构造访问接口连接地址 => 需要全部的采集端，包括内网的采集端...
    var theUrl = g_app.globalData.m_urlPrev + 'Mini/bindGather'
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
        // 调用接口失败 => 直接返回
        if (res.statusCode != 200) {
          strError = '发送绑定命令失败，错误码：' + res.statusCode
          that.setData({ m_show_feed: false, m_strError: strError })
          return
        }
        // dataType 没有设置json，需要自己转换...
        var arrData = JSON.parse(res.data);
        // 汇报反馈失败的处理 => 直接返回...
        if (arrData.err_code > 0) {
          strError = arrData.err_msg + ' 错误码：' + arrData.err_code
          that.setData({ m_show_feed: false, m_strError: strError })
          return
        }
        // 汇报状态成功 => 打印信息...
        console.log(thePostData)
        // 如果点了 确认绑定 或 取消，都直接跳转到 个人中心 页面...
        if (inSubCmd == BIND_SAVE || inSubCmd == BIND_CANCEL) {
          wx.switchTab({ url: '../gather/gather' })
        }
      },
      fail: function (res) {
        wx.hideLoading()
        wx.hideNavigationBarLoading()
      }
    })
  },
  // 用户点击重新扫码按钮...
  doScanCode: function () {
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
  // 点击“确认绑定”按钮的操作...
  doBindSave: function (inEvent) {
    this.doAPIBindMini(BIND_SAVE)
  },
  // 点击“取消”按钮的操作...
  doBindCancel: function (inEvent) {
    this.doAPIBindMini(BIND_CANCEL)
  }
})