<?php
/*************************************************************
    HaoYi (C)2017 - 2018 myhaoyi.com
    备注：专门处理第三方接口调用集合...
*************************************************************/

class APIAction extends Action
{
  public function _initialize() {
  }
  public function index() {
  }
  public function version() {
    print_r($_GET);
    $theEnc = $this->encrypt('oC8KG0irTOXqs41VpN5Nexo2_rRw');
    echo $theEnc . '<br>';
    $this->decrypt($theEnc);
  }
  function encrypt($id)
  {
    $id = serialize($id);
    $key = "1112121212121212121212";
    $data['expire'] = base64_encode(time(NULL)+3600);
    $data['value'] = openssl_encrypt($id, 'DES-ECB', $key, 0, base64_decode($data['expire']));
    $encrypt = base64_encode(json_encode($data));
    return $encrypt;
  }
  function decrypt($encrypt)
  {
    $key = '1112121212121212121212';//解密钥匙
    $encrypt = json_decode(base64_decode($encrypt), true);
    $iv = base64_decode($encrypt['expire']);
    $decrypt = openssl_decrypt($encrypt['value'], 'DES-ECB', $key, 0, $iv);
    $id = unserialize($decrypt);
    echo $id . '<br>' . ($iv-time(NULL)); 
  }  
}
?>