<?php
/**
 * Defines the main class Entity_Item
 */
class Oop_EntityItem {

	// Forward all requests for not found objects to the wrapper object.
	public function __get($prop) {
		return $this->wrapper->{$prop};
	}

	// Intercept all calls to methods not found on this object
	// and forward them to the wrapper object.
	public function __call($name, $arguments) {
		// forward the method calling
		return call_user_func_array(array($this->wrapper, $name),$arguments);
	}

	////////////////////////
	////// properties //////
	////////////////////////
	// the entity object
	private $entity;
	private $entity_type;

	// metadata wrapper
	public $wrapper;

	// the entity type properties name list
	private $property_name_list;

	// requirements data
	private $requirements;



	// the object
	private $item = array();


	public function __construct($entity_type, $entity, $requirements = NULL) {
		$this->entity = $entity;


		// build the wrapper
		$this->wrapper = entity_metadata_wrapper($entity_type, $entity);

		// build the entity property list
		$prop_info = entity_get_property_info( $entity_type );
		$this->property_name_list = array_keys($prop_info);

		// only do reading if $requirements is set
		if ($requirements) {
			$this->read($requirements);
		}
	}




	///////////////////////////////////
	////////// READING ////////////////
	///////////////////////////////////
	private function read($requirements) {
		// $requirements is an array containing 2 keys: fields and properties
		$this->get_properties( $requirements['properties'] );
		$this->get_fields( $requirements['fields'] );
	}

	private function get_properties($requirements_properties = NULL) {
		if ( empty($requirements_properties) ) { return false; }

		// get the requirements properties for the entity
		// and store them as public properties of the object

		foreach ($requirements_properties as $property_name => $options) {

			// define a basic property value
			$property_value = $this->wrapper->{$property_name}->value();

			////////////////////////////////////
			///////// PROP VALUE ALTER /////////
			////////////////////////////////////
			// define %propname_read_alter hook
			$data_alter_hook_name = $property_name .'_property_read';
			drupal_alter($data_alter_hook_name, $property_value, $options);

			// get alias
			$alias = !empty($options['alias']) ? $options['alias'] : $property_name;

			$this->item[ $alias ] = $property_value;
		}
	}

	private function get_fields($fields_options = NULL) {
		if ( empty($fields_options) ) { return false; }

		// get the requirements fields for the entity and store 
		// them in public properties of the new object.
		foreach ($fields_options AS $field_name => $options) {

			$field_info = field_info_field($field_name);
			$field_type = $field_info['type'];

			// define basic field value
			$field_value = $this->wrapper->{$field_name}->value();

			////////////////////////////////////
			///////// FIELD VALUE ALTER ////////
			////////////////////////////////////
			$data_alter_hook_name = $field_type .'_field_read';
			drupal_alter($data_alter_hook_name, $field_value, $options, $field_info);

			$alias = !empty($options['alias']) ? $options['alias'] : $field_name;

			$this->item[ $alias ] = $field_value;
		}
	}

	public function export() {
		return $this->item;
	}
	///////////////////////////////////
	////////// READING ////////////////
	///////////////////////////////////


	///////////////////////////////////
	////////// WRITING ////////////////
	///////////////////////////////////
	public function set($name, $value, $options = array(), $force = TRUE) {

		///////////////////////////////////////////
		/////// check if is a property name ///////
		///////////////////////////////////////////
		$is_property = array_search($name, $this->property_name_list);
		$is_property = !empty($is_property);


		///////////////////////////////////////////
		///////////// FIELD INFO //////////////////
		///////////////////////////////////////////
		if (!$is_property) {
			// it is a field
			$field_info = field_info_field($name);

			$cardinality = isset($field_info['cardinality']) ? $field_info['cardinality'] : 1;

		} else {
			// it is a property
			$cardinality = 1;
		}

		/////////////////////////////////
		////// DATA ALTERING HOOK ///////
		/////////////////////////////////
		$hook_suffix = ($is_property) ? '_property_formatter' : '_field_formatter';

		// define %method_%endpoint_data_alter hook
		$data_alter_hook_name =  $name . $hook_suffix;

		// @see user_module_invoke()
		foreach (module_implements($data_alter_hook_name) as $module) {
			$function = $module .'_'. $data_alter_hook_name;
			// will call all modules implementing hook_hook_name
			// and can pass each argument as reference determined
			// by the function declaration

			if ($is_property) {
				$function($value, $options);
			} else {
				$function($value, $options, $field_info);
			}
		}
		/////////////////////////////////
		////// DATA ALTERING HOOK ///////
		/////////////////////////////////


		/////////////////////////////////
		////////// SET! /////////////////
		/////////////////////////////////
		$field_is_single = $cardinality == 1;
		$value_is_single = is_array($value) ? array_values($value) !== $value : TRUE;

		if ($field_is_single && !$value_is_single) {
			// the field requires a single value, so just get the first value from the valuelist
			$value = array_shift($value);
		} else if (!$field_is_single && $value_is_single) {
			// the field requires a list but the $value is a single valyue single
			$value = array($value);
		}

		// set
		$this->wrapper->{$name}->set($value);

		return $this;
	}
}