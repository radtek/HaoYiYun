<?php
/*************************************************************
    HaoYi (C)2017 - 2020 myhaoyi.com

*************************************************************/

class GatherAction extends Action
{
  // 初始化页面的默认操作...
  public function _initialize() {
  }
  //
  // 计算时间差值...
  private function diffSecond($dStart, $dEnd)
  {
    $one = strtotime($dStart);//开始时间 时间戳
    $tow = strtotime($dEnd);//结束时间 时间戳
    $cle = $tow - $one; //得出时间戳差值
    return $cle; //返回秒数...
  }
  /**
  +--------------------------------------------------------------------
  * 验证采集端的合法性 => 是否取得授权继续运行...
  +--------------------------------------------------------------------
  */
  public function verify()
  {
    // 准备返回数据结构...
    $arrErr['err_code'] = false;
    $arrErr['err_msg'] = "OK";
    // 将获得的数据进行判断和处理 => 测试 => $_GET;
    $arrData = $_POST;
    do {
      // 判断输入数据是否有效...
      if( !isset($arrData['mac_addr']) || !isset($arrData['version']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = "MAC地址或版本号为空！";
        break;
      }
      // 判断输入的网站节点标记是否有效...
      if( !isset($arrData['node_tag']) || !isset($arrData['node_type']) || 
          !isset($arrData['node_addr']) || !isset($arrData['node_name']) ||
          !isset($arrData['node_proto']) )
      {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = "网站节点标记不能为空！";
        break;
      }
      // 根据node_addr判断，是互联网节点还是局域网节点...
      $arrAddr = explode(':', $arrData['node_addr']);
      $theIPAddr = gethostbyname($arrAddr[0]);
      $theWanFlag = (filter_var($theIPAddr, FILTER_VALIDATE_IP, FILTER_FLAG_NO_PRIV_RANGE | FILTER_FLAG_NO_RES_RANGE) ? true : false);
      // 根据节点标记获取或创建一条新记录...
      // 注意：node_addr已经包含了端口信息...
      $map['node_tag'] = $arrData['node_tag'];
      $dbNode = D('node')->where($map)->find();
      // 这里是通过元素个数判断，必须单独更新数据...
      if( count($dbNode) <= 0 ) {
        // 创建一条新纪录...
        $dbNode['node_wan'] = $theWanFlag;
        $dbNode['node_proto'] = $arrData['node_proto'];
        $dbNode['node_name'] = $arrData['node_name'];
        $dbNode['node_type'] = $arrData['node_type'];
        $dbNode['node_addr'] = $arrData['node_addr'];
        $dbNode['node_tag'] = $arrData['node_tag'];
        $dbNode['node_ver'] = $arrData['node_ver'];
        $dbNode['created'] = date('Y-m-d H:i:s');
        $dbNode['updated'] = date('Y-m-d H:i:s');
        $dbNode['node_id'] = D('node')->add($dbNode);
      } else {
        $dbNode['node_wan'] = $theWanFlag;
        $dbNode['node_proto'] = $arrData['node_proto'];
        $dbNode['node_name'] = $arrData['node_name'];
        $dbNode['node_type'] = $arrData['node_type'];
        $dbNode['node_addr'] = $arrData['node_addr'];
        $dbNode['node_tag'] = $arrData['node_tag'];
        $dbNode['node_ver'] = $arrData['node_ver'];
        $dbNode['updated'] = date('Y-m-d H:i:s');
        D('node')->save($dbNode);
      }
      // 判断获取的节点记录是否有效...
      if( $dbNode['node_id'] <= 0 ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = "网站节点编号无效！";
        break;
      }
      // 设定采集端所在的节点编号...
      $arrData['node_id'] = $dbNode['node_id'];
      // 根据MAC地址获取Gather记录信息...
      $where['mac_addr'] = $arrData['mac_addr'];
      $dbGather = D('gather')->where($where)->find();
      if( count($dbGather) <= 0 ) {
        // 没有找到记录，直接创建一个新记录 => 默认授权30天...
        $dbGather = $arrData;
        $dbGather['status'] = 1;
        $dbGather['created'] = date('Y-m-d H:i:s');
        $dbGather['updated'] = date('Y-m-d H:i:s');
        $dbGather['expired'] = date("Y-m-d H:i:s", strtotime("+30 days"));
        $arrErr['gather_id'] = D('gather')->add($dbGather);
        // 获取数据库设置的默认最大通道数...
        $condition['gather_id'] = $arrErr['gather_id'];
        $dbItem = D('gather')->where($condition)->field('max_camera')->find();
        // 准备返回数据 => 最大通道数、授权有效期、所在节点编号...
        $arrErr['auth_expired'] = $dbGather['expired'];
        $arrErr['max_camera'] = $dbItem['max_camera'];
        $arrErr['node_id'] = $arrData['node_id'];
      } else {
        // 查看用户授权是否已经过期 => 当前时间 与 过期时间 比较...
        $nDiffSecond = $this->diffSecond(date("Y-m-d H:i:s"), $dbGather['expired']);
        // 统一返回最大通道数、授权有效期、剩余天数...
        $arrErr['auth_days'] = ceil($nDiffSecond/3600/24);
        $arrErr['auth_expired'] = $dbGather['expired'];
        $arrErr['max_camera'] = $dbGather['max_camera'];
        // 授权过期，返回失败...
        if( $nDiffSecond <= 0 ) {
          $arrErr['err_code'] = true;
          $arrErr['err_msg'] = "授权已过期！";
          break;
        }
        // 授权有效，返回所在节点编号...
        $arrErr['gather_id'] = $dbGather['gather_id'];
        $arrErr['node_id'] = $arrData['node_id'];
        // 授权有效，将记录更新到数据库...
        $arrData['gather_id'] = $dbGather['gather_id'];
        $arrData['updated'] = date('Y-m-d H:i:s');
        $arrData['status'] = 1;
        D('gather')->save($arrData);
      }
    }while( false );
    // 直接返回运行结果 => json...
    echo json_encode($arrErr);
  }
  /**
  +--------------------------------------------------------------------
  * 处理采集端退出事件...
  +--------------------------------------------------------------------
  */
  public function logout()
  {
    // 判断输入的采集端编号是否有效...
    if( !isset($_POST['gather_id']) )
      return;
    // 将采集端自己的状态设置为0...
    $map['gather_id'] = $_POST['gather_id'];
    D('gather')->where($map)->setField('status', 0);
  }
}
?>