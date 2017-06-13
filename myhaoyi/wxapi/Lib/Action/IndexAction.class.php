<?php
/*************************************************************
    HaoYi (C)2017 - 2020 myhaoyi.com

*************************************************************/

class IndexAction extends Action
{
  public function _initialize() {
    // 创建一个新的移动检测对象...
    $this->m_detect = new Mobile_Detect();
    //////////////////////////////////////////////
    // 直接对页面进行跳转...
    // 电脑终端 => Home/index
    // 移动设备 => Mobile/index
    //////////////////////////////////////////////
    if( $this->m_detect->isMobile() ) {
      header("location:".__APP__.'/Mobile/index');
    } else {
      header("location:".__APP__.'/Home/index');
    }
  }
  /**
  +----------------------------------------------------------
  * 默认操作，进行页面分发...
  +----------------------------------------------------------
  */
  public function index()
  {
    //print $this->m_detect->getUserAgent();
    //print_r($this->m_detect->getHttpHeaders());
    //print_r($_COOKIE); exit;
  }
}
?>