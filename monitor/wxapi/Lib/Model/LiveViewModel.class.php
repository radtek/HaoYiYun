<?php

class LiveViewModel extends ViewModel {
    public $viewFields = array(
        'Camera'=>array('camera_id','gather_id','stream_prop','stream_url','stream_mp4','camera_type','camera_name','device_sn','device_ip','device_mac','status','clicks','created','updated','_type'=>'LEFT'), //Must be LEFT 返回左表所有数据，即使右表没有匹配的
        'Gather'=>array('gather_id','mac_addr','ip_addr','name_pc','name_set','_on'=>'Gather.gather_id=Camera.gather_id'),
    );
}
?>