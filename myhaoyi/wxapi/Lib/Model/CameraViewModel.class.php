<?php

class CameraViewModel extends ViewModel {
    public $viewFields = array(
        'Camera'=>array('camera_id','gather_id','grade_id','camera_type','camera_name','device_sn','device_ip','device_mac','clicks','created','updated','_type'=>'LEFT'), //Must be LEFT 返回左表所有数据，即使右表没有匹配的
        'Grade'=>array('grade_id','grade_type','grade_name','_on'=>'Grade.grade_id=Camera.grade_id','_type'=>'LEFT'),
    );
}
?>