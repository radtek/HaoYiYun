<?php
/*************************************************************
    HaoYi (C)2017 - 2020 myhaoyi.com

*************************************************************/

class GatherAction extends Action
{
  // 初始化页面的默认操作...
  public function _initialize() {
  }
  /**
  +--------------------------------------------------------------------
  * 处理默认index事件 => 添加或修改gather记录 => 返回系统配置信息...
  +--------------------------------------------------------------------
  */
  public function index()
  {
    // 准备返回数据结构...
    $arrErr['err_code'] = false;
    $arrErr['err_msg'] = "OK";
    // 将获得的数据进行判断和处理 => 测试 => $_GET;
    $arrData = $_POST;
    do {
      // 判断输入数据是否有效...
      if( !isset($arrData['mac_addr']) || !isset($arrData['ip_addr']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = "MAC地址或IP地址为空！";
        break;
      }
      // 根据MAC地址获取Gather记录信息...
      $map['mac_addr'] = $arrData['mac_addr'];
      $dbGather = D('gather')->where($map)->find();
      if( count($dbGather) <= 0 ) {
        // 没有找到记录，直接创建一个新记录...
        $arrData['status']  = 1;
        $arrData['created'] = date('Y-m-d H:i:s');
        $arrData['updated'] = date('Y-m-d H:i:s');
        $arrErr['gather_id'] = D('gather')->add($arrData);
        // 从数据库中再次获取新增的采集端记录...
        $condition['gather_id'] = $arrErr['gather_id'];
        $dbGather = D('gather')->where($condition)->find();
      } else {
        // 找到了记录，直接更新记录...
        $arrData['status']  = 1;
        $arrData['gather_id'] = $dbGather['gather_id'];
        $arrData['updated'] = date('Y-m-d H:i:s');
        D('gather')->save($arrData);
        // 准备需要返回的采集端编号...
        $arrErr['gather_id'] = $dbGather['gather_id'];
      }
      // 将采集端下面所有的通道状态设置成-1...
      $condition['gather_id'] = $arrErr['gather_id'];
      D('camera')->where($condition)->setField('status', -1);
      // 获取采集端下面所有的通道编号列表...
      $arrCamera = D('camera')->where($condition)->field('camera_id')->select();
      // 读取系统配置表，返回给采集端...
      $dbSys = D('system')->find();
      // 如果节点网站的标记为空，生成一个新的，并存盘...
      if( !$dbSys['web_tag'] ) {
        $dbSys['web_type'] = kCloudRecorder;
        $dbSys['web_tag'] = uniqid();
        $dbSave['system_id'] = $dbSys['system_id'];
        $dbSave['web_tag'] = $dbSys['web_tag'];
        D('system')->save($dbSave);
      }
      // 返回新增的采集端字段信息...
      $arrErr['name_set'] = $dbGather['name_set'];
      $arrErr['main_rate'] = $dbGather['main_rate'];
      $arrErr['sub_rate'] = $dbGather['sub_rate'];
      $arrErr['slice_val'] = $dbGather['slice_val'];
      $arrErr['inter_val'] = $dbGather['inter_val'];
      $arrErr['snap_val'] = $dbGather['snap_val'];
      $arrErr['auto_dvr'] = $dbGather['auto_dvr'];
      $arrErr['auto_fdfs'] = $dbGather['auto_fdfs'];
      // 返回采集端需要的参数配置信息...
      $arrErr['web_tag'] = $dbSys['web_tag'];
      $arrErr['web_type'] = $dbSys['web_type'];
      $arrErr['web_name'] = $dbSys['web_title'];
      $arrErr['tracker_addr'] = $dbSys['tracker_addr'];
      $arrErr['tracker_port'] = strval($dbSys['tracker_port']);
      $arrErr['transmit_addr'] = $dbSys['transmit_addr'];
      $arrErr['transmit_port'] = strval($dbSys['transmit_port']);
      $arrErr['local_time'] = date('Y-m-d H:i:s');
      $arrErr['camera'] = $arrCamera;
    }while( false );
    // 直接返回运行结果 => json...
    echo json_encode($arrErr);
  }
  /**
  +----------------------------------------------------------
  * 处理采集端获取配置 => 采集端在网站里的通用配置信息...
  +----------------------------------------------------------
  */
  /*public function getConfig()
  {
    // 准备返回数据结构...
    $arrErr['err_code'] = false;
    $arrErr['err_msg'] = "OK";
    // 将获得的数据进行判断和处理...
    $arrData = $_POST;
    do {
      // 判断输入数据是否有效...
      if( !isset($arrData['gather_id']) || !isset($arrData['mac_addr']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = "采集端编号或设备地址为空！";
        break;
      }
      // 查询对应的记录...
      $map['gather_id'] = $arrData['gather_id'];
      $map['mac_addr'] = $arrData['mac_addr'];
      $dbGather = D('gather')->where($map)->find();
      if( count($dbGather) <= 0 ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = "没有找到指定的采集端记录！";
        break;
      }
      // 读取系统配置表，返回给采集端...
      $dbSys = D('system')->find();
      // 将录像切片信息返回给强求采集端 => 后期可以根据需要继续添加...
      $arrErr['slice_val'] = strval($dbSys['slice_val']);
      $arrErr['inter_val'] = strval($dbSys['inter_val']);
    }while( false );
    // 直接返回运行结果 => json...
    echo json_encode($arrErr);
  }*/
  /**
  +----------------------------------------------------------
  * 处理采集端退出 => 将所有采集端下面的通道状态和采集端自己状态设置为0...
  +----------------------------------------------------------
  */
  public function logout()
  {
    // 判断输入的采集端编号是否有效...
    if( !isset($_POST['gather_id']) )
      return;
    // 将所有该采集端下面的通道状态设置为0...
    $map['gather_id'] = $_POST['gather_id'];
    D('camera')->where($map)->setField('status', 0);
    // 将采集端自己的状态设置为0...
    D('gather')->where($map)->setField('status', 0);
  }
  /**
  +----------------------------------------------------------
  * 获取通道配置信息和录像配置...
  +----------------------------------------------------------
  */
  public function getCamera()
  {
    // 准备返回数据结构...
    $arrErr['err_code'] = false;
    $arrErr['err_msg'] = "OK";
    // 将获得的数据进行判断和处理...
    $arrData = $_POST;
    do {
      // 判断输入数据是否有效...
      if( !isset($arrData['gather_id']) || !isset($arrData['camera_id']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = "采集端编号或通道编号为空！";
        break;
      }
      // 根据通道编号获取通道配置...
      $map['camera_id'] = $arrData['camera_id'];
      $dbCamera = D('camera')->where($map)->find();
      if( count($dbCamera) <= 0 ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = "没有找到指定通道的配置信息！";
        break;
      }
      // 2017.11.02 - by jackey => 去掉了年级信息...
      /*$dbCamera['grade_name'] = "";
      $dbCamera['grade_type'] = "";
      // 如果grade_id有效，则需要组合年级名称...
      if( $dbCamera['grade_id'] > 0 ) {
        $condition['grade_id'] = $dbCamera['grade_id'];
        $dbGrade = D('grade')->where($condition)->field('grade_type,grade_name')->find();
        $dbCamera['grade_name'] = $dbGrade['grade_name'];
        $dbCamera['grade_type'] = $dbGrade['grade_type'];
      }*/
      // 去掉一些采集端不需要的字段，减少数据量...
      unset($dbCamera['err_code']);
      unset($dbCamera['err_msg']);
      unset($dbCamera['created']);
      unset($dbCamera['updated']);
      unset($dbCamera['clicks']);
      // 将通道配置组合起来，反馈给采集端...
      $arrErr['camera'] = $dbCamera;
      // 读取该通道下的所有录像课程表，反馈给采集端...
      $arrCourse = D('course')->where($map)->select();
      // 将字符串时间转换成整数时间戳...
      foreach($arrCourse as &$dbItem) {
        $dbItem['start_time'] = strval(strtotime($dbItem['start_time']));
        $dbItem['end_time'] = strval(strtotime($dbItem['end_time']));
        unset($dbItem['created']); unset($dbItem['updated']);
      }
      // 将课程记录保存到返回队列当中...
      $arrErr['course'] = $arrCourse;      
    }while( false );
    // 直接返回运行结果 => json...
    echo json_encode($arrErr);
  }
  /**
  +----------------------------------------------------------
  * 处理摄像头注册事件 => 添加或修改camera记录...
  +----------------------------------------------------------
  */
  public function regCamera()
  {
    // 准备返回数据结构...
    $arrErr['err_code'] = false;
    $arrErr['err_msg'] = "OK";
    // 将获得的数据进行判断和处理...
    $arrData = $_POST;
    do {
      // 判断输入数据是否有效...
      if( !isset($arrData['gather_id']) || !isset($arrData['device_sn']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = "采集端编号或设备序列号为空！";
        break;
      }
      // 根据采集端编号和设备序列号获取Camera记录信息...
      $condition['gather_id'] = $arrData['gather_id'];
      $condition['device_sn'] = $arrData['device_sn'];
      $dbCamera = D('camera')->where($condition)->field('camera_id')->find();
      if( count($dbCamera) <= 0 ) {
        // 没有找到记录，直接创建一个新记录 => 返回camera_id...
        $dbCamera = $arrData;
        $dbCamera['created'] = date('Y-m-d H:i:s');
        $dbCamera['updated'] = date('Y-m-d H:i:s');
        $arrErr['camera_id'] = strval(D('camera')->add($dbCamera));
      } else {
        // 找到了记录，直接更新记录 => 返回camera_id...
        $arrErr['camera_id'] = strval($dbCamera['camera_id']);
        $arrData['camera_id'] = $dbCamera['camera_id'];
        $arrData['updated'] = date('Y-m-d H:i:s');
        D('camera')->save($arrData);
      }
      // 这里不用返回课程列表内容 => 只返回camera_id...
    }while( false );
    // 直接返回运行结果 => json...
    echo json_encode($arrErr);
  }
  /**
  +----------------------------------------------------------
  * 处理保存摄像头删除过程 => 返回 camera_id ...
  +----------------------------------------------------------
  */
  public function delCamera()
  {
    ////////////////////////////////////////////////////
    // 注意：用device_sn有点麻烦，最好改成camera_id...
    ////////////////////////////////////////////////////
    // 准备返回数据结构...
    $arrErr['err_code'] = false;
    $arrErr['err_msg'] = "OK";
    do {
      // 判断输入的 device_sn 是否有效...
      if( !isset($_POST['device_sn']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = "设备序列号为空！";
        break;
      }
      // 直接通过条件删除摄像头...
      $map['device_sn'] = $_POST['device_sn'];
      $dbCamera = D('camera')->where($map)->field('camera_id')->find();
      D('camera')->where($map)->delete();
      // 这里还需要删除对应的录像课程表记录...
      D('course')->where($dbCamera)->delete();
      // 删除通道下面所有的录像文件和截图...
      $arrList = D('RecordView')->where($dbCamera)->field('record_id,camera_id,file_fdfs,image_id,image_fdfs')->select();
      foreach ($arrList as &$dbVod) {
        // 删除图片和视频文件，逐一删除...
        fastdfs_storage_delete_file1($dbVod['file_fdfs']);
        fastdfs_storage_delete_file1($dbVod['image_fdfs']);
        // 删除图片记录和视频记录...
        D('record')->delete($dbVod['record_id']);
        D('image')->delete($dbVod['image_id']);
      }
      // 返回摄像头在数据库中的编号...
      $arrErr['camera_id'] = $dbCamera['camera_id'];
    }while( false );
    // 直接返回运行结果 => json...
    echo json_encode($arrErr);
  }
  /**
  +----------------------------------------------------------
  * 处理保存摄像头信息 => 参数 camera_id | status ...
  +----------------------------------------------------------
  */
  public function saveCamera()
  {
    /*// 准备返回数据结构...
    $arrErr['err_code'] = false;
    $arrErr['err_msg'] = "OK";
    // 将获得的数据进行判断和处理...
    $dbData = $_POST;
    do {
      // 判断输入数据是否有效...
      if( !isset($dbData['camera_id']) || !isset($dbData['status']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = "摄像头编号不能为空！";
        break;
      }
      // 直接保存摄像头数据...
      $arrErr['camera_id'] = $dbData['camera_id'];
      $dbData['updated'] = date('Y-m-d H:i:s');
      D('camera')->save($dbData);
    }while( false );
    // 直接返回运行结果 => json...
    echo json_encode($arrErr);*/

    // 直接保存，不返回结果...
    $_POST['updated'] = date('Y-m-d H:i:s');
    D('camera')->save($_POST);
  }
  /**
  +----------------------------------------------------------
  * 处理保存录像记录过程...
  +----------------------------------------------------------
  */
  public function saveFDFS()
  {
    // 准备返回数据结构...
    $arrErr['err_code'] = false;
    $arrErr['err_msg'] = "OK";
    // 将获得的数据进行判断和处理...$_GET;//
    $arrData = $_POST;
    do {
      // 判断输入数据是否有效...
      if( !isset($arrData['ext']) || !isset($arrData['file_src']) || !isset($arrData['file_fdfs']) || !isset($arrData['file_size']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = "文件名称或文件长度不能为空！";
        break;
      }
      // 将file_src进行切分...
      $arrSrc = explode('_', $arrData['file_src']);
      // $arrSrc[0] => uniqid
      // $arrSrc[1] => DBCameraID
      // 组合通用数据项...
      $arrData['file_src'] = (is_null($arrSrc[0]) ? $arrData['file_src'] : $arrSrc[0]);
      $arrData['camera_id'] = (is_null($arrSrc[1]) ? 0 : $arrSrc[1]);
      $arrData['created'] = date('Y-m-d H:i:s'); // mp4的创建时间 => $arrSrc[2]
      $arrData['updated'] = date('Y-m-d H:i:s');
      // 根据文件扩展名进行数据表分发...
      // jpg => uniqid_DBCameraID
      // mp4 => uniqid_DBCameraID_CreateTime_CourseID_Duration
      if( (strcasecmp($arrData['ext'], ".jpg") == 0) || (strcasecmp($arrData['ext'], ".jpeg") == 0) ) {
        // 如果是直播截图，进行特殊处理...
        if( strcasecmp($arrData['file_src'], "live") == 0 ) {
          $map['camera_id'] = $arrData['camera_id'];
          // 找到该通道下的截图路径和截图编号...
          $dbLive = D('LiveView')->where($map)->field('camera_id,image_id,image_fdfs')->find();
          // 如果找到了有效通道...
          if( is_array($dbLive) ) {
            if( $dbLive['image_id'] > 0 ) {
              // 通道下的截图是有效的，先删除这个截图的物理存在...
              if( isset($dbLive['image_fdfs']) && strlen($dbLive['image_fdfs']) > 0 ) { 
                fastdfs_storage_delete_file1($dbLive['image_fdfs']);
              }
              // 将新的截图存储路径更新到截图表当中...
              $dbImage['image_id'] = $dbLive['image_id'];
              $dbImage['file_fdfs'] = $arrData['file_fdfs'];
              $dbImage['file_size'] = $arrData['file_size'];
              $dbImage['updated'] = $arrData['updated'];
              D('image')->save($dbImage);
              // 返回这个有效的图像记录编号...
              $arrErr['image_id'] = $dbImage['image_id'];
            } else {
              // 通道下的截图是无效的，创建新的截图记录...
              $arrErr['image_id'] = D('image')->add($arrData);
              $dbLive['image_id'] = $arrErr['image_id'];
              // 将新的截图记录更新到通道表中...
              D('camera')->save($dbLive);
            }
          }
        } else {
          // 是录像截图，查找截图记录...
          $dbImage = D('image')->where('file_src="'.$arrData['file_src'].'"')->find();
          if( is_array($dbImage) ) {
            // 更新截图记录...
            $dbImage['camera_id'] = $arrData['camera_id'];
            $dbImage['file_fdfs'] = $arrData['file_fdfs'];
            $dbImage['file_size'] = $arrData['file_size'];
            $dbImage['updated'] = $arrData['updated'];
            D('image')->save($dbImage);
            // 返回这个有效的图像记录编号...
            $arrErr['image_id'] = $dbImage['image_id'];
          } else {
            // 新增截图记录...
            $arrErr['image_id'] = D('image')->add($arrData);
          }
          // 在录像表中查找file_src，找到了，则更新image_id，截图匹配...
          $dbRec = D('record')->where('file_src="'.$arrData['file_src'].'"')->field('record_id,image_id')->find();
          if( is_array($dbRec) ) {
            $dbRec['image_id'] = $arrErr['image_id'];
            $dbRec['updated'] = $arrData['updated'];
            D('record')->save($dbRec);
          }
        }
      } else if( strcasecmp($arrData['ext'], ".mp4") == 0 ) {
        // 保存录像时长(秒)，初始化image_id...
        // $arrSrc[2] => CreateTime
        // $arrSrc[3] => CourseID
        // $arrSrc[4] => Duration
        $arrData['image_id'] = 0;
        $arrData['course_id'] = (is_null($arrSrc[3]) ? 0 : $arrSrc[3]);
        $arrData['duration'] = (is_null($arrSrc[4]) ? 0 : $arrSrc[4]);
        $nTotalSec = intval($arrData['duration']);
        $theSecond = intval($nTotalSec % 60);
        $theMinute = intval($nTotalSec / 60) % 60;
        $theHour = intval($nTotalSec / 3600);
        if( $theHour > 0 ) {
          $arrData['disptime'] = sprintf("%02d:%02d:%02d", $theHour, $theMinute, $theSecond);
        } else {
          $arrData['disptime'] = sprintf("%02d:%02d", $theMinute, $theSecond);
        }
        // 如果course_id有效，则需要查找到subject_id和teacher_id...
        // 如果course_id已经被删除了，设置默认的subject_id和teacher_id都为1，避免没有编号，造成前端无法显示的问题...
        if( $arrData['course_id'] > 0 ) {
          $dbCourse = D('course')->where('course_id='.$arrData['course_id'])->field('subject_id,teacher_id')->find();
          $arrData['subject_id'] = (isset($dbCourse['subject_id']) ? $dbCourse['subject_id'] : 1);
          $arrData['teacher_id'] = (isset($dbCourse['teacher_id']) ? $dbCourse['teacher_id'] : 1);
        }
        // 在图片表中查找file_src，找到了，则新增image_id，截图匹配...
        $dbImage = D('image')->where('file_src="'.$arrData['file_src'].'"')->field('image_id,camera_id')->find();
        if( is_array($dbImage) ) {
          $arrData['image_id'] = $dbImage['image_id'];
        }
        // 查找录像记录...
        $dbRec = D('record')->where('file_src="'.$arrData['file_src'].'"')->find();
        if( is_array($dbRec) ) {
          // 更新录像记录...
          $dbRec['subject_id'] = $arrData['subject_id'];
          $dbRec['teacher_id'] = $arrData['teacher_id'];
          $dbRec['image_id'] = $arrData['image_id'];
          $dbRec['camera_id'] = $arrData['camera_id'];
          $dbRec['file_fdfs'] = $arrData['file_fdfs'];
          $dbRec['file_size'] = $arrData['file_size'];
          $dbRec['duration'] = $arrData['duration'];
          $dbRec['disptime'] = $arrData['disptime'];
          $dbRec['updated'] = $arrData['updated'];
          D('record')->save($dbRec);
          $arrErr['record_id'] = $dbRec['record_id'];
        } else {
          // 新增录像记录 => 创建时间用传递过来的时间...
          $arrData['created'] = date('Y-m-d H:i:s', $arrSrc[2]);
          $arrErr['record_id'] = D('record')->add($arrData);
        }
      }
    }while( false );
    // 直接返回运行结果 => json...
    echo json_encode($arrErr);
  }
}
?>