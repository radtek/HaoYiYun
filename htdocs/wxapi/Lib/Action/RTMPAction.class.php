<?php
/*************************************************************
    HaoYi (C)2017 - 2020 myhaoyi.com

*************************************************************/

class RTMPAction extends Action
{
  // 初始化页面的默认操作...
  public function _initialize() {
  }
  //
  // 处理 login 接口...
  public function login()
  {
    $this->doTransmit(kCmd_Live_Login);
  }
  //
  // 处理 quit 接口...
  public function quit()
  {
    $this->doTransmit(kCmd_Live_Quit);
  }
  //
  // 处理 vary 接口...
  public function vary()
  {
    $this->doTransmit(kCmd_Live_Vary);
  }
  //
  // 统一的转发命令接口...
  private function doTransmit($nCommand)
  {
    // 判断输入参数的有效性...
    // rtmp_addr => 192.168.1.180:1935
    // rtmp_live => 0(login) => >0(vary) => <0(quit)
    // rtmp_user => 0
    $arrData = $_POST;
    if( !isset($arrData['rtmp_addr']) || !isset($arrData['rtmp_live']) || !isset($arrData['rtmp_user']) )
      return false;
    
    // 尝试链接中转服务器...
    $dbSys = D('system')->field('transmit_addr,transmit_port')->find();
    
    /*// 尝试链接中转服务器 => 发送直播登录命令...
    $saveJson = json_encode($arrData);
    $json_data = php_transmit_command($dbSys['transmit_addr'], $dbSys['transmit_port'], kClientLive, $nCommand, $saveJson);
    // 反馈转发结果...
    echo $json_data;*/

    // 通过php扩展插件连接中转服务器 => 性能高...
    $transmit = transmit_connect_server($dbSys['transmit_addr'], $dbSys['transmit_port']);
    if( !$transmit ) return false;
    // 尝试链接中转服务器 => 发送直播登录命令...
    $saveJson = json_encode($arrData);
    $json_data = transmit_command(kClientLive, $nCommand, $transmit, $saveJson);
    // 关闭中转服务器链接...
    transmit_disconnect_server($transmit);
    // 反馈转发结果...
    echo $json_data;
  }
  //
  // 处理 rtmp 服务器在发布时的用户信息验证...
  public function publish()
  {
    // for nginx-rtmp...
    //logdebug(json_encode($_POST));
    //header('HTTP/1.0 404 Not Found');
    //header('HTTP/1.0 200 OK');
    
    // for srs...
    logdebug(file_get_contents("php://input"));
    echo '0';
  }
}
?>