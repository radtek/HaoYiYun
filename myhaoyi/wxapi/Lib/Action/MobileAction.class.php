<?php
/*************************************************************
    Wan (C)2016 - 2017 myhaoyi.com
    备注：专门处理手机端请求页面...
*************************************************************/

class MobileAction extends Action
{
  public function _initialize() {
    $this->m_detect = new Mobile_Detect();
    // 如果不是移动端访问，直接跳转到PC端...
    if( !$this->m_detect->isMobile() ) {
      header("location:".__APP__.'/Index/index');
      return;
    }
  }
  /**
  +----------------------------------------------------------
  * 默认操作 - 手机端页面显示...
  +----------------------------------------------------------
  */
  public function index()
  {
    // 手机终端页面显示...
    $this->display('index');
  }
}
?>