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
      if( !isset($arrData['node_tag']) || !isset($arrData['node_type']) || !isset($arrData['node_addr']) || !isset($arrData['node_name']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = "网站节点标记不能为空！";
        break;
      }
      // 根据节点标记获取或创建一条新记录...
      $map['node_tag'] = $arrData['node_tag'];
      $dbNode = D('node')->where($map)->find();
      if( count($dbNode) <= 0 ) {
        // 创建一条新纪录...
        $dbNode['node_name'] = $arrData['node_name'];
        $dbNode['node_type'] = $arrData['node_type'];
        $dbNode['node_addr'] = $arrData['node_addr'];
        $dbNode['node_tag'] = $arrData['node_tag'];
        $dbNode['created'] = date('Y-m-d H:i:s');
        $dbNode['updated'] = date('Y-m-d H:i:s');
        $dbNode['node_id'] = D('node')->add($dbNode);
      } else {
        $dbNode['node_name'] = $arrData['node_name'];
        $dbNode['node_type'] = $arrData['node_type'];
        $dbNode['node_addr'] = $arrData['node_addr'];
        $dbNode['node_tag'] = $arrData['node_tag'];
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
        $dbGather['created'] = date('Y-m-d H:i:s');
        $dbGather['updated'] = date('Y-m-d H:i:s');
        $dbGather['expired'] = date("Y-m-d H:i:s", strtotime("+30 days"));
        $arrErr['gather_id'] = D('gather')->add($dbGather);
      } else {
        // 查看用户授权是否已经过期 => 当前时间 与 过期时间 比较...
        $nDiffSecond = $this->diffSecond(date("Y-m-d H:i:s"), $dbGather['expired']);
        if( $nDiffSecond <= 0 ) {
          $arrErr['err_code'] = true;
          $arrErr['err_msg'] = "授权已过期！";
          break;
        }
        // 授权有效，直接更新记录...
        $arrErr['gather_id'] = $dbGather['gather_id'];
        $arrData['gather_id'] = $dbGather['gather_id'];
        $arrData['updated'] = date('Y-m-d H:i:s');
        D('gather')->save($arrData);
      }
    }while( false );
    // 直接返回运行结果 => json...
    echo json_encode($arrErr);
  }
}
?>