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
	private $property_info;
	private $property_name_list;


	// the entity field name list
	private $field_info;
	private $field_name_list;

	// requirements data
	private $requirements;



	// the object
	private $item = array();


	public function __construct($data) {
		/*
			$data = array(
				$entity => object,
				$entity_type => string,
				$entity_bundle => string,
				$requirements => array
			);
		*/

		$this->entity_type = isset($data['entity_type']) ? $data['entity_type'] : NULL;
		$this->entity_bundle = isset($data['entity_bundle']) ? $data['entity_bundle'] : NULL;
		$this->requirements = isset($data['requirements']) ? $data['requirements'] : array();



		/////////////////////
		//// BUILD ENTITY ///
		/////////////////////
		if (!empty($data['entity'])) {
			$this->entity = $data['entity'];
		} else {
			// data used to create the entity
			$entity_data = array();

			// discover the key to the entitty bundle value
			$entity_info = entity_get_info($this->entity_type);

			if (!empty($entity_info['entity keys']['bundle']) && !empty($this->entity_bundle)) {
				$entity_data[ $entity_info['entity keys']['bundle'] ] = $this->entity_bundle;
			}

			$this->entity = entity_create($this->entity_type, $entity_data);
		}
		/////////////////////
		//// BUILD ENTITY ///
		/////////////////////

		///////////////////////
		// build the wrapper //
		///////////////////////
		$this->wrapper = entity_metadata_wrapper($this->entity_type, $this->entity);

		////////////////////////////////////
		// build the entity property list //
		////////////////////////////////////
		$this->property_info = entity_get_property_info( $this->entity_type );
		$this->property_name_list = array_keys($this->property_info['properties']);

		// only do reading if $requirements is set
		if (!empty($this->requirements) && !empty($this->entity)) {
			$this->read($this->requirements);
		}
	}




	///////////////////////////////////
	////////// READING ////////////////
	///////////////////////////////////
	public function read($requirements = FALSE) {
		if (empty($requirements)) {
			$requirements = $this->requirements;
		}

		// $requirements is an array containing 2 keys: fields and properties
		if (isset($requirements['properties'])) {
			$this->get_properties( $requirements['properties'] );
		}

		if (isset($requirements['fields'])) {
			$this->get_fields( $requirements['fields'] );
		}
	}

	private function get_properties($requirement_properties = FALSE) {
		if ( empty($requirement_properties) ) { return false; }

		// get the requirements properties for the entity
		// and store them as public properties of the object

		foreach ($requirement_properties as $property_name => $options) {
			// $options must be an array
			$options = is_array($options) ? $options : array();

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

	private function get_fields($requirement_fields = FALSE) {
		if ( empty($requirement_fields) ) { return false; }

		// get the requirements fields for the entity and store 
		// them in public properties of the new object.
		foreach ($requirement_fields AS $field_name => $options) {

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


		//////////////////////////
		/////// SINGLE OR NOT ////
		//////////////////////////
		$field_is_single = $cardinality == 1;
		$value_is_single = is_array($value) ? array_values($value) !== $value : TRUE;
		$options['field_is_single'] = $field_is_single;
		$options['value_is_single'] = $value_is_single;

		/////////////////////////////////
		////// DATA ALTERING HOOK ///////
		/////////////////////////////////
		// define _data_alter hook
		$data_alter_hook_name = ($is_property) ? $name.'_property_formatter' : $field_info['type'].'_field_formatter';

		// @see user_module_invoke()
		foreach (module_implements($data_alter_hook_name) as $module) {
			$alter_function = $module .'_'. $data_alter_hook_name;
			// will call all modules implementing hook_hook_name
			// and can pass each argument as reference determined
			// by the function declaration


			// if the value is a list of values, iterate over the list
			if (!$value_is_single) {

				$value_list = $value;
				$value = array();

				// iterate over value list
				foreach ($value_list as $v) {
					if ($is_property) {
						$v = $alter_function($v, $options);
					} else {
						$v = $alter_function($v, $options, $field_info);
					}

					array_push($value, $v);
				}

			} else {
				if ($is_property) {
					$value = $alter_function($value, $options);
				} else {
					$value = $alter_function($value, $options, $field_info);
				}
			}
		}

		/////////////////////////////////
		////// DATA ALTERING HOOK ///////
		/////////////////////////////////


		/////////////////////////////////
		////////// SET! /////////////////
		/////////////////////////////////
		// only set if the value is not NULL
		if (!is_null($value)) {
			if ($field_is_single && !$value_is_single) {
				// the field requires a single value, so just get the first value from the valuelist
				$value = array_shift($value);
			} else if (!$field_is_single && $value_is_single) {
				// the field requires a list but the $value is a single valyue single
				$value = array($value);
			}
			
			// set
			$this->wrapper->{$name}->set($value);			
		}


		// after data is set, the object must be updated
		$this->read($this->requirements);

		return $this;
	}
	///////////////////////////////////
	////////// WRITING ////////////////
	///////////////////////////////////


	///////////////////////////////////
	//////////// INFO /////////////////
	///////////////////////////////////
	public function info($what) {

		switch ($what) {
			case 'property_info':
				return $this->property_info;
				break;

			case 'properties':
				return $this->property_name_list;
				break;

			case 'field_info':
				if (empty($this->field_info)) {
					$this->build_field_info();
				}

				return $this->field_info;
				break;

			case 'fields':
				if (empty($this->field_name_list)) {
					$this->build_field_info();
				}

				return $this->field_name_list;
				break;
			
			default:
				# code...
				break;
		}
	}

	private function build_field_info() {
		/////////////////////////////
		// build entity field list //
		/////////////////////////////
		if (empty($this->entity_bundle)) {
			$entity_info = entity_get_info($this->entity_type);

			if (empty($entity_info['entity keys']['bundle'])) {
				$this->entity_bundle = $this->entity_type;
			} else {
				$this->entity_bundle = $this->entity->{$entity_info['entity keys']['bundle']};
			}
		}


		// build the entity field name list
		// field_info_instances($entity_type = NULL, $bundle_name = NULL)
		$this->field_info = field_info_instances($this->entity_type, $this->entity_bundle);
		$this->field_name_list = array_keys($this->field_info);
	}
	///////////////////////////////////
	//////////// INFO /////////////////
	///////////////////////////////////
}