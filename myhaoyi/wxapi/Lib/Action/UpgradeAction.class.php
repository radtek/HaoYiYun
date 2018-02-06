<?php
/*************************************************************
    Wan (C)2018- 2020 myhaoyi.com
    备注：专门处理升级请求页面...
*************************************************************/

class UpgradeAction extends Action
{
  public function _initialize() {
  }
  // 读取文件列表并返回...
  /*public function upDbFile()
  {
    // 获取需要读取的文件和目录...
    $theName = $_GET['name'];
    $theDBPath = ($_GET['type'] > 0 ? '/weike/mysql/data/monitor' : '/weike/mysql/data/haoyi');
    // 组合文件完整路径名称...
    $theFile = sprintf("%s/%s", $theDBPath, $theName);
    if( file_exists($theFile) ) {
     $theData = file_get_contents($theFile);
    }
    // 返回读取到的文件数据...
    echo $theData;
  }*/
  //
  // 获取单个数据表的结构内容...
  public function upDbTable()
  {
    // 准备返回结果...
    $arrErr['err_code'] = false;
    $arrErr['err_msg'] = 'ok';
    // 注意：这里是 $_POST 参数...
    do {
      // 判断输入参数的有效性...
      if( !isset($_POST['name']) || !isset($_POST['type']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '输入的参数无效';
        break;
      }
      // 获取输入的数据表名称 => 必须指定数据库名称...
      $dbName = ($_POST['type'] > 0 ? 'monitor' : 'haoyi');
      $tbName = substr($_POST['name'], 0, strlen($_POST['name'])-4);
      // 获取数据表的结构内容 => 注意要指定数据库名称，否则查询当前数据库...
      $theDB = Db::getInstance();
      $theSQL = sprintf("show full columns from %s.%s", $dbName, $tbName);
      $arrErr['fields'] = $theDB->query($theSQL);
      // 注意：这里要返回数据表名称...
      $arrErr['table'] = $tbName;
    } while( false );
    // 返回json数据包...
    echo json_encode($arrErr);
  }
  //
  // 获取数据库升级列表文件...
  public function upDbase()
  {
    // 得到对应数据库的目录 => 打开目录，定义返回数组...
    $theDBPath = ($_GET['type'] > 0 ? '/weike/mysql/data/monitor' : '/weike/mysql/data/haoyi');
    $handler = opendir($theDBPath);
    $arrList = array();
    // 遍历数据库目录下的文件列表...
    while( ($fileName = readdir($handler)) !== false ) {
      // 分割文件名 => 不带目录...
      $arrName = pathinfo($fileName);
      // 如果是表结构文件，获取更详细文件信息...
      if( strcasecmp($arrName['extension'], 'frm') == 0 ) {
        // 组合表结构文件的全路径，获取文件修改时间...
        $theFullPath = sprintf("%s/%s", $theDBPath, $fileName);
        $theMTime = date("Y-m-d H:i:s", filemtime($theFullPath));
        // 得到文件的名称、大小、修改时间...
        $dbFile['name'] = $arrName['basename'];
        $dbFile['size'] = filesize($theFullPath);
        $dbFile['time'] = $theMTime;
        // 将表结构信息写入数组当中...
        array_push($arrList, $dbFile);
      }
    }
    // 关闭目录句柄...
    closedir($handler);
    // 返回数据表的文件列表..
    echo json_encode($arrList);
  }
}
?>