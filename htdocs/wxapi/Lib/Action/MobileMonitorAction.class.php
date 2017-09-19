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
  // 获取最大页码...
  private function fetchMaxPage()
  {
    // 通过传递过来的科目编号进行数据分发...
    $theSubjectID = $_GET['subject_id'];
    switch( $theSubjectID ) {
      case -1: $map['Record.record_id'] = array('gt', 0); break;
      case -2: $map['Camera.camera_id'] = array('gt', 0); break;
      default: $map['Record.subject_id'] = $theSubjectID; break;
    }
    // 获取每页记录数...
    $pagePer = C('PAGE_PER');
    if( $theSubjectID == -2 ) {
      $totalNum = D('LiveView')->where($map)->count();
    } else {
      $totalNum = D('RecordView')->where($map)->count();
    }
    // 计算总页数...
    $max_page = intval($totalNum / $pagePer);
    // 判断是否是整数倍的页码...
    $max_page += (($totalNum % $pagePer) ? 1 : 0);
    return $max_page;
  }
  //
  // 获取最新swiper接口...
  private function fetchArrNewVod()
  {
    // 通过传递过来的科目编号进行数据分发...
    $theSubjectID = $_GET['subject_id'];
    switch( $theSubjectID ) {
      case -1: $map['Record.record_id'] = array('gt', 0); break;
      case -2: $map['Camera.camera_id'] = array('gt', 0); break;
      default: $map['Record.subject_id'] = $theSubjectID; break;
    }
    // 筛选出最新的5个直播或点播节目...
    if( $theSubjectID == -2 ) {
      // 获取最新的5个直播节目...
      $arrRecord = D('LiveView')->where($map)->limit(5)->order('Camera.created DESC')->select();
      // 获取服务器地址 => http 或 https
      $strAddr = "http://";
      if((isset($_SERVER['HTTPS']) && $_SERVER['HTTPS'] == 'on') || (isset($_SERVER['REQUEST_SCHEME']) && $_SERVER['REQUEST_SCHEME'] == 'https')) {
        $strAddr = "https://";
      }
      // 重新组合swiper需要的数据内容...
      foreach($arrRecord as $key => &$dbItem) {
        $strLiveImg = (($dbItem['status'] <= 0) ? "wxapi/public/images/live-off.png" : "wxapi/public/images/live-on.png");
        $theImgUrl = sprintf("%s%s:%d/%s", $strAddr, $_SERVER['HTTP_HOST'], $_SERVER['SERVER_PORT'], $strLiveImg);
        $theTitle = sprintf("%s %s %s", $dbItem['grade_type'], $dbItem['grade_name'], $dbItem['camera_name']);
        $arrNewVod[$key] = array('id' => $dbItem['camera_id'], 'url' => 'javascript:', 'img' => $theImgUrl, 'title' => $theTitle);
      }
    } else {
      // 获取最新的5个点播节目...
      $arrRecord = D('RecordView')->where($map)->limit(5)->order('Record.created DESC')->select();
      // 获取图片链接需要的数据 => web_tracker_addr 已经自带了协议头 http://或https://
      $dbSys = D('system')->field('web_tracker_addr,web_tracker_port')->find();
      // 重新组合swiper需要的数据内容...
      foreach($arrRecord as $key => &$dbItem) {
        $theImgUrl = sprintf("%s:%d/%s", $dbSys['web_tracker_addr'], $dbSys['web_tracker_port'], $dbItem['image_fdfs']);
        $theTitle = sprintf("%s %s %s %s", $dbItem['grade_type'], $dbItem['subject_name'], $dbItem['teacher_name'], $dbItem['title_name']);
        $arrNewVod[$key] = array('id' => $dbItem['record_id'], 'url' => 'javascript:', 'img' => $theImgUrl, 'title' => $theTitle);
      }
    }
    return $arrNewVod;
  }
  //
  // 获取分页gallery接口...
  private function fetchArrGallery()
  {
    // 通过传递过来的科目编号进行数据分发...
    $theSubjectID = $_GET['subject_id'];
    switch( $theSubjectID ) {
      case -1: $map['Record.record_id'] = array('gt', 0); break;
      case -2: $map['Camera.camera_id'] = array('gt', 0); break;
      default: $map['Record.subject_id'] = $theSubjectID; break;
    }
    // 得到每页条数...
    $pagePer = C('PAGE_PER');
    $pageCur = (isset($_GET['p']) ? $_GET['p'] : 1);  // 当前页码...
    $pageLimit = (($pageCur-1)*$pagePer).','.$pagePer; // 读取范围...
    // 读取直播或点播分页记录数据...
    if( $theSubjectID == -2 ) {
      // 获取服务器地址...
      $strAddr = "http://";
      if((isset($_SERVER['HTTPS']) && $_SERVER['HTTPS'] == 'on') || (isset($_SERVER['REQUEST_SCHEME']) && $_SERVER['REQUEST_SCHEME'] == 'https')) {
        $strAddr = "https://";
      }
      // 获取直播分页数据，并对数据进行重新组合...
      $arrGallery = D('LiveView')->where($map)->limit($pageLimit)->order('Camera.created DESC')->select();
      foreach($arrGallery as &$dbItem) {
        $strLiveImg = (($dbItem['status'] <= 0) ? "wxapi/public/images/live-off.png" : "wxapi/public/images/live-on.png");
        $dbItem['image_fdfs'] = sprintf("%s%s:%d/%s", $strAddr, $_SERVER['HTTP_HOST'], $_SERVER['SERVER_PORT'], $strLiveImg);
        $dbItem['subject_name'] = $dbItem['grade_name'];
        $dbItem['teacher_name'] = $dbItem['camera_name'];
        $dbItem['title_name'] = $dbItem['school_name'];
      }
    } else {
      // 获取图片链接需要的数据 => web_tracker_addr 已经自带了协议头 http://或https://
      $dbSys = D('system')->field('web_tracker_addr,web_tracker_port')->find();
      // 获取点播分页数据，并对缩略图片进行地址重组...
      $arrGallery = D('RecordView')->where($map)->limit($pageLimit)->order('Record.created DESC')->select();
      // 组合需要返回的数据 => web_tracker_addr 已经自带了协议头 http://或https://
      foreach($arrGallery as &$dbItem) {
        $dbItem['image_fdfs'] = sprintf("%s:%d/%s", $dbSys['web_tracker_addr'], $dbSys['web_tracker_port'], $dbItem['image_fdfs']);
        $dbItem['file_fdfs'] = sprintf("%s:%d/%s", $dbSys['web_tracker_addr'], $dbSys['web_tracker_port'], $dbItem['file_fdfs']);
      }
    }
    return $arrGallery;
  }
  //
  // 得到科目列表...
  // 参数：subject_id => 科目编号...
  public function getSubject()
  {
    // 指定其它域名访问内容 => 跨域访问...
    header('Access-Control-Allow-Origin:*');
    // 筛选出所有的科目列表，在数组头部加入两个元素...
    $arrSubject = D('subject')->field('subject_id,subject_name')->order('subject_id ASC')->select();
    array_unshift($arrSubject, array('subject_id' => -1, 'subject_name' => '最新'), array('subject_id' => -2, 'subject_name' => '直播'));
    // 组合需要的返回数据块...
    $arrData['maxGalPage'] = $this->fetchMaxPage();
    $arrData['arrGallery'] = $this->fetchArrGallery();
    $arrData['arrNewVod'] = $this->fetchArrNewVod();
    $arrData['arrSubject'] = $arrSubject;
    // 返回json编码数据包...
    echo json_encode($arrData);
  }
  //
  // 得到分页记录...
  // 参数：subject_id => 科目编号
  public function getGallery()
  {
    // 指定其它域名访问内容 => 跨域访问...
    header('Access-Control-Allow-Origin:*');
    // 通过接口函数获取分页数据...
    $arrGallery = $this->fetchArrGallery();
    // 返回json编码数据包...
    echo json_encode($arrGallery);
  }
  //
  // 得到swiper数据内容 => 5 条最新数据...
  // 参数：subject_id => 科目编号
  public function getSwiper()
  {
    // 指定其它域名访问内容 => 跨域访问...
    header('Access-Control-Allow-Origin:*');
    // 通过接口函数获取分页数据...
    $arrNewVod = $this->fetchArrNewVod();
    // 返回json编码数据包...
    echo json_encode($arrNewVod);
  }
}
?>