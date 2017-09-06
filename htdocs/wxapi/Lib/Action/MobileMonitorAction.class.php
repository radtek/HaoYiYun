<?php
/*************************************************************
    HaoYi (C)2017 - 2020 myhaoyi.com

*************************************************************/

class MobileMonitorAction extends Action
{
  public function _initialize() {
    // 创建一个新的移动检测对象...
    /*$this->m_detect = new Mobile_Detect();
    //////////////////////////////////////////////
    // 电脑端访问了移动端页面，直接对页面进行跳转...
    //////////////////////////////////////////////
    if( !$this->m_detect->isMobile() ) {
      header("location:".__APP__.'/Home/index');
    }*/
  }
  /**
  +----------------------------------------------------------
  * 默认操作，进行页面分发...
  +----------------------------------------------------------
  */
  public function index()
  {
    echo "Mobile-Cloud-Monitor";
  }
  //
  // 得到科目列表...
  public function getSubject()
  {
    // 得到每页条数，总记录数，计算总页数...
    $pagePer = C('PAGE_PER');
    $totalNum = D('RecordView')->count();
    $max_page = intval($totalNum / $pagePer);
    // 判断是否是整数倍的页码...
    $max_page += (($totalNum % $pagePer) ? 1 : 0);
    // 筛选出所有的科目列表，在数组头部加入两个元素...
    $arrSubject = D('subject')->field('subject_id,subject_name')->order('subject_id ASC')->select();
    array_unshift($arrSubject, array('subject_id' => -1, 'subject_name' => '最新'), array('subject_id' => -2, 'subject_name' => '直播'));
    // 筛选出最新的5个最新的点播节目...
    $arrRecord = D('RecordView')->limit(5)->order('Record.created DESC')->select();
    // 获取图片链接需要的数据 => web_tracker_addr 已经自带了协议头 http://或https://
    $dbSys = D('system')->field('web_tracker_addr,web_tracker_port')->find();
    $arrNewVod = array();
    foreach($arrRecord as $key => &$dbItem) {
      $theImgUrl = sprintf("%s:%d/%s", $dbSys['web_tracker_addr'], $dbSys['web_tracker_port'], $dbItem['image_fdfs']);
      $theTitle = sprintf("%s %s %s %s", $dbItem['grade_type'], $dbItem['subject_name'], $dbItem['teacher_name'], $dbItem['title_name']);
      $arrNewVod[$key] = array('id' => $dbItem['record_id'], 'url' => 'javascript:', 'img' => $theImgUrl, 'title' => $theTitle);
    }
    // 筛选出最新的10个最新的点播节目...
    $arrGallery = D('RecordView')->limit($pagePer)->order('Record.created DESC')->select();
    // 组合需要返回的数据 => web_tracker_addr 已经自带了协议头 http://或https://
    $arrData['webTracker'] = sprintf("%s:%d", $dbSys['web_tracker_addr'], $dbSys['web_tracker_port']);
    $arrData['maxGalPage'] = $max_page;
    $arrData['arrGallery'] = $arrGallery;
    $arrData['arrSubject'] = $arrSubject;
    $arrData['arrNewVod'] = $arrNewVod;
    // 指定其它域名访问内容 => 跨域访问...
    header('Access-Control-Allow-Origin:*');
    echo json_encode($arrData);
  }
  //
  // 得到分页记录...
  public function getGallery()
  {
    // 得到每页条数...
    $pagePer = C('PAGE_PER');
    $pageCur = (isset($_GET['p']) ? $_GET['p'] : 1);  // 当前页码...
    $pageLimit = (($pageCur-1)*$pagePer).','.$pagePer; // 读取范围...
    $arrGallery = D('RecordView')->limit($pageLimit)->order('Record.created DESC')->select();
    // 指定其它域名访问内容 => 跨域访问...
    header('Access-Control-Allow-Origin:*');
    echo json_encode($arrGallery);
  }
}
?>