<?php
/*************************************************************
    Wan (C)2015 - 2016 happyhope.net
    备注：专门处理微信事件分发的代码
*************************************************************/

class IndexAction extends Action
{
  public function _initialize() {
    //$this->m_detect = new Mobile_Detect();
  }
  /**
  +----------------------------------------------------------
  * 默认操作 - 处理全部非微信端的访问...
  +----------------------------------------------------------
  */
  public function index()
  {
    // 处理第三方网站扫码登录的授权回调过程 => 其它网站的登录，state != 32...
    if( isset($_GET['code']) && isset($_GET['state']) && strlen($_GET['state']) != 32 ) {
      A('Login')->doWechatAuth($_GET['code'], $_GET['state']);
      return;
    }
    ///////////////////////////////////////////////////
    // 以后，这里还可以对移动设备进行界面优化...
    ///////////////////////////////////////////////////
    // $this->detect->isMobile();
    // print $this->detect->getUserAgent();
    // print_r($this->detect->getHttpHeaders());

    //print_r($_COOKIE); exit;
    
    echo 'Hello';
  }
}
?>