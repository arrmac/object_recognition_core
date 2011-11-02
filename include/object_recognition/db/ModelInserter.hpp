#pragma once

#include <ecto/ecto.hpp>

#include "object_recognition/common/json_spirit/json_spirit_reader_template.h"
#include "object_recognition/common/types.h"
#include "object_recognition/db/db.h"
#include "object_recognition/db/view_types.h"

namespace
{
  /** Function that compares the intersection of two
   * @param obj1
   * @param obj2
   * @return true if the intersection between the keys have the same values
   */
  bool
  CompareJsonIntersection(const json_spirit::mObject &obj1, const json_spirit::mObject &obj2)
  {
//TODO
    return true;
  }
}

namespace object_recognition
{
  namespace db
  {
    namespace bases
    {
      struct ModelInserterUtils
      {
        static Document
        populate_doc(const ObjectDb& db, const CollectionName& collection_name, const ObjectId& object_id,
                     const std::string& model_params, const std::string& model_type);
      };
      /** Class inserting the arbitrary Models into the DB
       */
      template<typename T>
      struct ModelInserter: T
      {
        typedef ModelInserter<T> C;

        static void
        declare_params(ecto::tendrils& params)
        {
          params.declare(&C::collection_name_, "collection", //
                         "std::string The collection in which to store the models on the db", //
                         "object_recognition").required(true);
          params.declare(&C::db_params_, "db_params", //
                         "The DB parameters").required(true);
          params.declare(&C::object_id_, "object_id", //
                         "The object id, to associate this frame with.").required(true);
          params.declare(&C::model_params_, "model_json_params", //
                         "The parameters used for the model, as JSON.").required(true);
          T::declare_params(params);
        }

        static void
        declare_io(const ecto::tendrils& params, ecto::tendrils& inputs, ecto::tendrils& outputs)
        {
          T::declare_io(params, inputs, outputs);
        }

        void
        configure(const ecto::tendrils& params, const ecto::tendrils& inputs, const ecto::tendrils& outputs)
        {
          db_.set_params(*db_params_);
          T::configure(params, inputs, outputs);
        }

        int
        process(const ecto::tendrils& inputs, const ecto::tendrils& outputs)
        {
          Document doc_new = ModelInserterUtils::populate_doc(db_, *collection_name_, *object_id_, *model_params_,
                                                              T::model_type());
          json_spirit::mObject in_parameters;
          {
            json_spirit::mValue value;
            json_spirit::read(*model_params_, value);
            in_parameters = value.get_obj();
          }

          std::cout << "persisting " << doc_new.id() << std::endl;
          int rval = T::process(inputs, outputs, doc_new);
          if (rval == ecto::OK)
          {
            // Find all the models of that type for that object
            object_recognition::db::View view(object_recognition::db::View::VIEW_MODEL_WHERE_OBJECT_ID_AND_MODEL_TYPE);
            view.Initialize(*object_id_, T::model_type());
            ViewIterator view_iterator(view, db_, *collection_name_);

            ViewIterator iter = view_iterator.begin(), end = view_iterator.end();
            for (; iter != end; ++iter)
            {
              // Compare the parameters
              json_spirit::mObject db_parameters = (*iter).get_value<json_spirit::mObject>("parameters");

              // If they are the same, delete the current model in the database
              if (CompareJsonIntersection(in_parameters, db_parameters))
              {
                db_.Delete((*iter).id(), *collection_name_);
                break;
              }
            }

            doc_new.Persist();
          }
          std::cout << "done persisting " << doc_new.id() << std::endl;
          return rval;
        }
      private:
        ObjectDb db_;
        ecto::spore<ObjectDbParameters> db_params_;
        ecto::spore<DocumentId> object_id_;
        ecto::spore<CollectionName> collection_name_;
        ecto::spore<std::string> model_params_;
      };
    }
  }
}