<?php
/*************************************************************
    HaoYi (C)2017 - 2020 myhaoyi.com

*************************************************************/

class GatherAction extends Action
{
  // 测试获取小程序二维码...
  /*public function qrcode()
  {
    $strTokenUrl = 'https://api.weixin.qq.com/cgi-bin/token?grant_type=client_credential&appid=wx78b419f717fb1552&secret=29605607d5005fa5f1a5dde59eeb42eb';
    $result = http_get($strTokenUrl);
    $json = json_decode($result,true);
    //echo $json['access_token'];
    
    $strQueryUrl = 'https://api.weixin.qq.com/wxa/getwxacodeunlimit?access_token='.$json['access_token'];
    $arrPost['scene'] = '100';
    $arrPost['page'] = 'pages/index/index';
    $strPost = json_encode($arrPost);
    $result = http_post($strQueryUrl, $strPost);
    print_r($result);
  }*/
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
        $dbSys['web_type'] = kCloudEducate;
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
      $arrErr['auto_ipc'] = $dbGather['auto_ipc'];
      $arrErr['page_size'] = $dbGather['page_size'];
      $arrErr['selected'] = LIVE_BEGIN_ID + $dbGather['live_id'];
      $arrErr['begin'] = LIVE_BEGIN_ID;
      // 返回采集端需要的参数配置信息...
      $arrErr['web_ver'] = C('VERSION');
      $arrErr['web_tag'] = $dbSys['web_tag'];
      $arrErr['web_type'] = $dbSys['web_type'];
      $arrErr['web_name'] = $dbSys['web_title'];
      $arrErr['local_time'] = date('Y-m-d H:i:s');
      $arrErr['camera'] = $arrCamera;
      // 注意：tracker_addr|remote_addr|udp_addr，已经通过loginLiveRoom获取到了...
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
      // 1. 直接通过条件删除摄像头...
      $map['device_sn'] = $_POST['device_sn'];
      $dbCamera = D('camera')->where($map)->field('camera_id')->find();
      D('camera')->where($map)->delete();
      // 2. 这里还需要删除对应的录像课程表记录...
      D('course')->where($dbCamera)->delete();
      // 3. 删除通道下面所有的录像文件和截图...
      $arrList = D('RecordView')->where($dbCamera)->field('record_id,camera_id,file_fdfs,image_id,image_fdfs')->select();
      foreach ($arrList as &$dbVod) {
        // 删除图片和视频文件，逐一删除...
        fastdfs_storage_delete_file1($dbVod['file_fdfs']);
        fastdfs_storage_delete_file1($dbVod['image_fdfs']);
        // 删除图片记录和视频记录...
        D('record')->delete($dbVod['record_id']);
        D('image')->delete($dbVod['image_id']);
      }
      // 4.1 找到通道下所有的图片记录...
      $arrImage = D('image')->where($dbCamera)->field('image_id,camera_id,file_fdfs')->select();
      // 4.2 删除通道快照图片的物理存在 => 逐一删除...
      foreach($arrImage as &$dbImage) {
        fastdfs_storage_delete_file1($dbImage['file_fdfs']);
      }
      // 4.3 删除通道快照图片记录的数据库存在 => 一次性删除...
      D('image')->where($dbCamera)->delete();
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
  //
  // 处理教师端上传保存...
  public function liveFDFS()
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
      // $arrSrc[1] => RoomID
      // 组合通用数据项...
      $arrData['file_src'] = (is_null($arrSrc[0]) ? $arrData['file_src'] : $arrSrc[0]);
      $arrData['live_id'] = (is_null($arrSrc[1]) ? 0 : $arrSrc[1]);
      $arrData['created'] = date('Y-m-d H:i:s'); // mp4的创建时间 => $arrSrc[2]
      $arrData['updated'] = date('Y-m-d H:i:s');
      // 根据文件扩展名进行数据表分发...
      // jpg => uniqid_RoomID
      if( (strcasecmp($arrData['ext'], ".jpg") == 0) || (strcasecmp($arrData['ext'], ".jpeg") == 0) ) {
        // 如果是直播截图，进行特殊处理...
        if( strcasecmp($arrData['file_src'], "live") == 0 ) {
          // 重新计算直播间的数据库编号，并在数据库中查找...
          $arrData['live_id'] = intval($arrData['live_id']) - LIVE_BEGIN_ID;
          $condition['live_id'] = $arrData['live_id'];
          $dbLive = D('LiveView')->where($condition)->field('live_id,lesson_id,image_id,image_fdfs')->find();
          // 如果找到了有效通道...
          if( is_array($dbLive) ) {
            if( $dbLive['image_id'] > 0 ) {
              // 通道下的截图是有效的，先删除这个截图的物理存在...
              if( isset($dbLive['image_fdfs']) && strlen($dbLive['image_fdfs']) > 0 ) { 
                if( !fastdfs_storage_delete_file1($dbLive['image_fdfs']) ) {
                  logdebug("fdfs delete failed => ".$dbLive['image_fdfs']);
                }
              }
              // 将新的截图存储路径更新到截图表当中...
              $dbImage['image_id'] = $dbLive['image_id'];
              $dbImage['lesson_id'] = $dbLive['lesson_id'];
              $dbImage['file_fdfs'] = $arrData['file_fdfs'];
              $dbImage['file_size'] = $arrData['file_size'];
              $dbImage['updated'] = $arrData['updated'];
              D('image')->save($dbImage);
              // 返回这个有效的图像记录编号...
              $arrErr['image_id'] = $dbImage['image_id'];
            } else {
              // 通道下的截图是无效的，创建新的截图记录...
              $arrData['lesson_id'] = $dbLive['lesson_id'];
              $arrErr['image_id'] = D('image')->add($arrData);
              $dbLive['image_id'] = $arrErr['image_id'];
              // 将新的截图记录更新到课程表中...
              $dbLesson['lesson_id'] = $dbLive['lesson_id'];
              $dbLesson['image_id'] = $dbLive['image_id'];
              D('lesson')->save($dbLesson);
            }
          }
        }
      }
    } while ( false );
    // 直接返回运行结果 => json...
    echo json_encode($arrErr);
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
      // mp4 => uniqid_DBCameraID_CreateTime_CourseID_SliceID_SliceInx_Duration
      if( (strcasecmp($arrData['ext'], ".jpg") == 0) || (strcasecmp($arrData['ext'], ".jpeg") == 0) ) {
        // 如果是直播截图，进行特殊处理...
        if( strcasecmp($arrData['file_src'], "live") == 0 ) {
          $map['camera_id'] = $arrData['camera_id'];
          // 找到该通道下的截图路径和截图编号...
          $dbLive = D('CameraView')->where($map)->field('camera_id,image_id,image_fdfs')->find();
          // 如果找到了有效通道...
          if( is_array($dbLive) ) {
            if( $dbLive['image_id'] > 0 ) {
              // 通道下的截图是有效的，先删除这个截图的物理存在...
              if( isset($dbLive['image_fdfs']) && strlen($dbLive['image_fdfs']) > 0 ) { 
                if( !fastdfs_storage_delete_file1($dbLive['image_fdfs']) ) {
                  logdebug("fdfs delete failed => ".$dbLive['image_fdfs']);
                }
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
          $condition['file_src'] = $arrData['file_src'];
          $dbImage = D('image')->where($condition)->find();
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
          $dbRec = D('record')->where($condition)->field('record_id,image_id')->find();
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
        // $arrSrc[4] => SliceID
        // $arrSrc[5] => SliceInx
        // $arrSrc[6] => Duration
        $arrData['image_id'] = 0;
        $arrData['course_id'] = (is_null($arrSrc[3]) ? 0 : $arrSrc[3]);
        $arrData['slice_id'] = (is_null($arrSrc[4]) ? NULL : $arrSrc[4]);
        $arrData['slice_inx'] = (is_null($arrSrc[5]) ? 0 : $arrSrc[5]);
        $arrData['duration'] = (is_null($arrSrc[6]) ? 0 : $arrSrc[6]);
        $nTotalSec = intval($arrData['duration']);
        $theSecond = intval($nTotalSec % 60);
        $theMinute = intval($nTotalSec / 60) % 60;
        $theHour = intval($nTotalSec / 3600);
        if( $theHour > 0 ) {
          $arrData['disptime'] = sprintf("%02d:%02d:%02d", $theHour, $theMinute, $theSecond);
        } else {
          $arrData['disptime'] = sprintf("%02d:%02d", $theMinute, $theSecond);
        }
        // 云监控模式，需要去掉subject_id和teacher_id...
        // 如果course_id有效，则需要查找到subject_id和teacher_id...
        // 如果course_id已经被删除了，设置默认的subject_id和teacher_id都为1，避免没有编号，造成前端无法显示的问题...
        /*if( $arrData['course_id'] > 0 ) {
          $dbCourse = D('course')->where('course_id='.$arrData['course_id'])->field('subject_id,teacher_id')->find();
          $arrData['subject_id'] = (isset($dbCourse['subject_id']) ? $dbCourse['subject_id'] : 1);
          $arrData['teacher_id'] = (isset($dbCourse['teacher_id']) ? $dbCourse['teacher_id'] : 1);
        }*/
        // 在图片表中查找file_src，找到了，则新增image_id，截图匹配...
        $condition['file_src'] = $arrData['file_src'];
        $dbImage = D('image')->where($condition)->field('image_id,camera_id')->find();
        if( is_array($dbImage) ) {
          $arrData['image_id'] = $dbImage['image_id'];
        }
        // 查找通道记录，获取采集端编号...
        $map['camera_id'] = $arrData['camera_id'];
        $dbCamera = D('camera')->where($map)->field('camera_id,gather_id')->find();
        // 查找录像记录，判断是添加还是修改...
        $dbRec = D('record')->where($condition)->find();
        if( is_array($dbRec) ) {
          // 更新录像记录 => 云监控模式，需要去掉subject_id和teacher_id...
          //$dbRec['subject_id'] = $arrData['subject_id'];
          //$dbRec['teacher_id'] = $arrData['teacher_id'];
          $dbRec['image_id'] = $arrData['image_id'];
          $dbRec['camera_id'] = $arrData['camera_id'];
          $dbRec['gather_id'] = $dbCamera['gather_id'];
          $dbRec['file_fdfs'] = $arrData['file_fdfs'];
          $dbRec['file_size'] = $arrData['file_size'];
          $dbRec['duration'] = $arrData['duration'];
          $dbRec['disptime'] = $arrData['disptime'];
          $dbRec['course_id'] = $arrData['course_id'];
          $dbRec['slice_id'] = $arrData['slice_id'];
          $dbRec['slice_inx'] = $arrData['slice_inx'];
          $dbRec['updated'] = $arrData['updated'];
          D('record')->save($dbRec);
          $arrErr['record_id'] = $dbRec['record_id'];
          // 更新创建时间，以便后续统一使用...
          $arrData['created'] = $dbRec['created'];
        } else {
          // 新增录像记录 => 创建时间用传递过来的时间...
          $arrData['gather_id'] = $dbCamera['gather_id'];
          $arrData['created'] = date('Y-m-d H:i:s', $arrSrc[2]);
          $arrErr['record_id'] = D('record')->add($arrData);
        }
        // 检测并触发通道的过期删除过程...
        $this->doCheckExpire($arrData['camera_id'], $arrData['created']);
      }
    }while( false );
    // 直接返回运行结果 => json...
    echo json_encode($arrErr);
  }
  //
  // 检测并触发通道的过期删除过程...
  private function doCheckExpire($inCameraID, $inCreated)
  {
    // 查找系统配置记录...
    $dbSys = D('system')->find();
    // 如果不处理删除操作，直接返回...
    if( $dbSys['web_save_days'] <= 0 )
      return;
    // 时间从当前时间向前推进指定天数...
    $strFormat = sprintf("%s -%d days", $inCreated, $dbSys['web_save_days']);
    $strExpire = date('Y-m-d H:i:s', strtotime($strFormat));
    // 准备查询条件内容 => 小于过期内容...
    $condition['camera_id'] = $inCameraID;
    $condition['created'] = array('lt', $strExpire);
    // 删除该通道下对应的过期录像文件、录像截图...
    $arrList = D('RecordView')->where($condition)->field('record_id,camera_id,file_fdfs,image_id,image_fdfs,created')->select();
    foreach ($arrList as &$dbVod) {
      // 删除图片和视频文件，逐一删除...
      fastdfs_storage_delete_file1($dbVod['file_fdfs']);
      fastdfs_storage_delete_file1($dbVod['image_fdfs']);
      // 删除图片记录和视频记录...
      D('record')->delete($dbVod['record_id']);
      D('image')->delete($dbVod['image_id']);
    }
  }
  // 删除事件测试函数...
  /*public function testCheck()
  {
    $inCameraID = 16;
    $inCreated = "2018-04-15 20:24:52";
    $dbSys = D('system')->find();
    // 如果不处理删除操作，直接返回...
    if( $dbSys['web_save_days'] <= 0 )
      return;
    // 时间从当前时间向前推进指定天数...
    $strFormat = sprintf("%s -%d days", $inCreated, $dbSys['web_save_days']);
    $strExpire = date('Y-m-d H:i:s', strtotime($strFormat));
    // 准备查询条件内容 => 小于过期内容...
    $condition['camera_id'] = $inCameraID;
    $condition['created'] = array('lt', $strExpire);
    // 删除该通道下对应的过期录像文件、录像截图...
    $arrList = D('RecordView')->where($condition)->field('record_id,camera_id,file_fdfs,image_id,image_fdfs,created')->select();
    print_r($arrList);
  }*/
  //
  // 获取直播间列表事件...
  public function getLiveRoom()
  {
    // 准备返回数据结构...
    $arrErr['err_code'] = false;
    $arrErr['err_msg'] = "OK";
    do {
      // 传递过来的采集端编号是否有效...
      $condition['gather_id'] = $_POST['gather_id'];
      $dbGather = D('gather')->where($condition)->field('gather_id,live_id')->find();
      if( !isset($dbGather['gather_id']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '没有找到指定的采集端！';
        break;
      }
      // 返回已经挂载的直播间编号...
      $arrErr['selected'] = strval(LIVE_BEGIN_ID + $dbGather['live_id']);
      // 查询当前所有的直播间列表...
      $arrLive = D('LiveView')->order('live_id DESC')->field('live_id,lesson_name,teacher_name,start_time,end_time')->select();
      // 如果没有直播间，返回错误...
      if( !is_array($arrLive) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '服务器上没有直播间！';
        break;
      }
      // 如果找到了有效的直播间，设置直播间编号的偏移量...
      $arrErr['begin'] = strval(LIVE_BEGIN_ID);
      $arrErr['live'] = $arrLive;
    } while( false );
    // 直接反馈获取的数据内容信息...
    echo json_encode($arrErr);
  }
  //
  // 将指定的采集端挂载到指定的直播间...
  public function setLiveRoom()
  {
    // 准备返回数据结构...
    $arrErr['err_code'] = false;
    $arrErr['err_msg'] = "OK";
    do {
      // 判断输入参数是否有效...
      if( !isset($_POST['gather_id']) || !isset($_POST['room_id']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '输入参数有误！';
        break;
      }
      // 计算需要挂载的直播间的数据库的编号...
      $nLiveID = intval($_POST['room_id']) - LIVE_BEGIN_ID;
      if( $nLiveID <= 0 ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '输入参数有误！';
        break;
      }
      // 准备数据，执行挂载操作...
      $dbGather['gather_id'] = $_POST['gather_id'];
      $dbGather['live_id'] = $nLiveID;
      D('gather')->save($dbGather);
    } while( false );
    // 直接反馈获取的数据内容信息...
    echo json_encode($arrErr);
  }
  //
  // 处理来自学生端或讲师端的云教室登录事件...
  public function loginLiveRoom()
  {
    // 准备返回数据结构...
    $arrErr['err_code'] = false;
    $arrErr['err_msg'] = "OK";
    // 将获得的数据进行判断和处理...$_GET;//
    $arrPost = $_POST;
    do {
      // 判断输入参数是否有效...
      if( !isset($arrPost['room_id']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '请输入有效的云教室号码，号码从200000开始！';
        break;
      }
      // 计算有效的的直播间的数据库的编号...
      $nLiveID = intval($arrPost['room_id']) - LIVE_BEGIN_ID;
      if( $nLiveID <= 0 ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '请输入有效的云教室号码，号码从200000开始！';
        break;
      }
      // 验证云教室是否存在...
      $condition['live_id'] = $nLiveID;
      $dbLive = D('live')->where($condition)->field('live_id')->find();
      if( !isset($dbLive['live_id']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '没有找到指定的云教室号码，请确认后重新输入！';
        break;
      }
      // 验证发送的终端类型是否正确...
      $nClientType = intval($arrPost['type_id']);
      if(($nClientType != kClientStudent) && ($nClientType != kClientTeacher)) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '不是合法的终端类型，请确认后重新登录';
        break;
      }
      // 2018.07.27 - 新的UDP服务器，不需要rtmp地址信息 => 暂时不要删除这段代码...
      // 构造中转服务器需要的参数 => 直播编号 => LIVE_BEGIN_ID + live_id
      /*$dbParam['rtmp_live'] = LIVE_BEGIN_ID + $dbLive['live_id'];
      // 从中转服务器获取云教室直播链接地址...
      $dbResult = $this->getRtmpUrlFromTransmit($dbParam);
      // 如果获取连接中转服务器失败...
      if( $dbResult['err_code'] > 0 ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = $dbResult['err_msg'];
        break;
      }
      // 将获取到的rtmp地址进行特殊处理...
      $strRtmpUrl = $dbResult['rtmp_url'];
      $strRule = '/(^[rR][tT][mM][pP]:\/\/.*\/[lL][iI][vV][eE]\/)(.*)$/';
      preg_match($strRule, $strRtmpUrl, $arrMatch);
      if( empty($arrMatch) || !is_array($arrMatch) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '直播地址错误 => ' . $strRtmpUrl;
        break;
      }
      // 将分解后的直播地址反馈给讲师端...
      $arrErr['live_server'] = $arrMatch[1];
      $arrErr['live_key'] = $arrMatch[2];*/
      
      // 读取系统配置数据库记录...
      $dbSys = D('system')->find();
      // 构造UDP中心服务器需要的参数 => 房间编号 => LIVE_BEGIN_ID + live_id
      $dbParam['room_id'] = LIVE_BEGIN_ID + $dbLive['live_id'];
      // 从UDP中心服务器获取UDP直播地址和UDP中转地址...
      $dbResult = $this->getUdpServerFromUdpCenter($dbSys['udpcenter_addr'], $dbSys['udpcenter_port'], $dbParam);
      // 如果获取连接中转服务器失败...
      if( $dbResult['err_code'] > 0 ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = $dbResult['err_msg'];
        break;
      }
      // 注意：需要将数字转换成字符串...
      // 填充跟踪服务器的地址和端口...
      $arrErr['tracker_addr'] = $dbSys['tracker_addr'];
      $arrErr['tracker_port'] = strval($dbSys['tracker_port']);
      // 填充udp远程服务器的地址和端口 => 从UDPCenter获取的来自UDPServer的汇报...
      $arrErr['remote_addr'] = $dbResult['remote_addr'];
      $arrErr['remote_port'] = strval($dbResult['remote_port']);
      // 填充udp服务器的地址和端口 => 从UDPCenter获取的来自UDPServer的汇报...
      $arrErr['udp_addr'] = $dbResult['udp_addr'];
      $arrErr['udp_port'] = strval($dbResult['udp_port']);
      // 获取当前指定通道上的讲师端和学生端在线数量...
      $arrErr['teacher'] = strval($dbResult['teacher']);
      $arrErr['student'] = strval($dbResult['student']);
      // 如果是讲师端登录，修改房间在线状态...
      if( $nClientType == kClientTeacher ) {
        $dbLive['status'] = 1;
        D('live')->save($dbLive);
      }
    } while( false );
    // 直接反馈最终验证的结果...
    echo json_encode($arrErr);
  }
  // 从udp中心服务器获取udp中转服务器和udp直播服务器地址...
  // 成功 => array()
  // 失败 => false
  private function getUdpServerFromUdpCenter($inUdpCenterAddr, $inUdpCenterPort, &$dbParam)
  {
    // 通过php扩展插件连接中转服务器 => 性能高...
    $transmit = transmit_connect_server($inUdpCenterAddr, $inUdpCenterPort);
    // 链接中转服务器失败，直接返回...
    if( !$transmit ) {
      $arrData['err_code'] = true;
      $arrData['err_msg'] = '无法连接直播中心服务器。';
      return $arrData;
    }
    // 获取当前房间所在UDP直播服务器地址和端口、中转服务器地址和端口...
    $saveJson = json_encode($dbParam);
    $json_data = transmit_command(kClientPHP, kCmd_PHP_GetUdpServer, $transmit, $saveJson);
    // 关闭中转服务器链接...
    transmit_disconnect_server($transmit);
    // 获取的JSON数据有效，转成数组，直接返回...
    $arrData = json_decode($json_data, true);
    if( !$arrData ) {
      $arrData['err_code'] = true;
      $arrData['err_msg'] = '从直播中心服务器获取数据失败。';
      return $arrData;
    }
    // 通过错误码，获得错误信息...
    $arrData['err_msg'] = getTransmitErrMsg($arrData['err_code']);
    // 将整个数组返回...
    return $arrData;
  }
  //
  // 处理学生端或老师端退出事件...
  public function logoutLiveRoom()
  {
    // 判断输入的云教室号码是否有效...
    if( !isset($_POST['room_id']) || !isset($_POST['type_id']) )
      return;
    // 获取终端类型...
    $nClientType = intval($_POST['type_id']);
    // 计算有效的的直播间的数据库的编号...
    $nLiveID = intval($_POST['room_id']) - LIVE_BEGIN_ID;
    // 如果是讲师端退出，修改房间为离线状态...
    if( $nClientType == kClientTeacher ) {
      $dbSave['live_id'] = $nLiveID;
      $dbSave['status'] = 0;
      // 直接进行数据库操作...
      D('live')->save($dbSave);
    }
  }  
  //
  // 获取UDPCenter的地址和端口 => UDPServer调用的接口...
  public function getUDPCenter()
  {
    // 获取系统配置信息...
    $dbSys = D('system')->find();
    // 准备返回数据结构...
    $arrErr['err_code'] = false;
    $arrErr['err_msg'] = "OK";
    // 填充UDPCenter的地址和端口...
    $arrErr['udpcenter_addr'] = $dbSys['udpcenter_addr'];
    $arrErr['udpcenter_port'] = $dbSys['udpcenter_port'];
    // 直接反馈查询结果...
    echo json_encode($arrErr);   
  }
  // 从中转服务器获取直播地址...
  // 成功 => array()
  // 失败 => false
  /*private function getRtmpUrlFromTransmit(&$dbParam)
  {
    // 获取系统配置信息...
    $dbSys = D('system')->find();
    // 通过php扩展插件连接中转服务器 => 性能高...
    $transmit = transmit_connect_server($dbSys['transmit_addr'], $dbSys['transmit_port']);
    // 链接中转服务器失败，直接返回...
    if( !$transmit ) {
      $arrData['err_code'] = true;
      $arrData['err_msg'] = '无法连接中转服务器。';
      return $arrData;
    }
    // 获取直播频道所在的URL地址...
    $saveJson = json_encode($dbParam);
    $json_data = transmit_command(kClientPlay, kCmd_Play_Login, $transmit, $saveJson);
    // 关闭中转服务器链接...
    transmit_disconnect_server($transmit);
    // 获取的JSON数据有效，转成数组，直接返回...
    $arrData = json_decode($json_data, true);
    if( !$arrData ) {
      $arrData['err_code'] = true;
      $arrData['err_msg'] = '从中转服务器获取数据失败。';
      return $arrData;
    }
    // 通过错误码，获得错误信息...
    $arrData['err_msg'] = getTransmitErrMsg($arrData['err_code']);
    // 将整个数组返回...
    return $arrData;
  }
  //
  // 获取指定云教室里面的在线摄像头列表...
  public function getRoomCameraList()
  {
    // 准备返回数据结构...
    $arrErr['err_code'] = false;
    $arrErr['err_msg'] = "OK";
    do {
      // 判断输入参数是否有效...
      if( !isset($_POST['room_id']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '请输入有效的云教室号码，号码从200000开始！';
        break;
      }
      // 计算有效的的直播间的数据库的编号...
      $nLiveID = intval($_POST['room_id']) - LIVE_BEGIN_ID;
      if( $nLiveID <= 0 ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '请输入有效的云教室号码，号码从200000开始！';
        break;
      }
      // 设置查询条件 => CameraView 当中status是指wk_camera表...
      $condition['live_id'] = $nLiveID;
      $condition['status'] = array('gt', 0);
      $arrErr['camera'] = D('CameraView')->where($condition)->field('camera_id,camera_name,name_pc,name_set')->select();
    } while( false );
    // 直接反馈查询结果...
    echo json_encode($arrErr);   
  }
  //
  // 获取指定在线摄像头的直播地址 => 触发按需推流机制...
  public function getRoomCameraUrl()
  {
    // 准备返回数据结构...
    $arrErr['err_code'] = false;
    $arrErr['err_msg'] = "OK";
    do {
      // 没有找到指定通道，返回错误...
      $condition['camera_id'] = $_POST['camera_id'];
      $dbCamera = D('CameraView')->where($condition)->field('camera_id,mac_addr')->find();
      if( !isset($dbCamera['camera_id']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '没有找到指定的摄像头数据！';
        break;
      }
      // 从中转服务器获取摄像头的直播地址，并激发按需推流...
      $dbParam['mac_addr'] = $dbCamera['mac_addr'];
      $dbParam['rtmp_live'] = $dbCamera['camera_id'];
      // 获取直播链接地址...
      $dbResult = $this->getRtmpUrlFromTransmit($dbParam);
      // 如果获取连接中转服务器失败...
      if( $dbResult['err_code'] > 0 ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = $dbResult['err_msg'];
        break;
      }
      // 保存需要返回的 rtmp 直播地址和用户编号...
      $arrErr['rtmp_url'] = $dbResult['rtmp_url'];
      $arrErr['player_id'] = $dbResult['player_id'];
    } while( false );
    // 直接反馈查询结果...
    echo json_encode($arrErr);   
  }*/
}
?>